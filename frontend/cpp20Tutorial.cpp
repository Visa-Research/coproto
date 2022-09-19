#include "cpp20Tutorial.h"
#include "coproto/coproto.h"
#include <string>
#include "tests/Tests.h"
//#include "coproto/Executor/ThreadPool.h"


using namespace coproto;

#ifdef COPROTO_CPP20

namespace {

    ///////////////////////////////////////////////////
    //             Echo Server Example
    //-------------------------------------------------
    // This example shows how to send a message and get
    // the same message back.
    //
    // coproto is based on the idea of coroutines which 
    // are functions that can "pause" and later be resumed.
    // While this is not needed in this example, we will
    // see in future examples the benefits it can offer.
    //
    // The key thing to understand at this point is the
    // need to call co_await on each socket operation. 
    // This gives the coproto framework an opportunity
    // to start the IO operations and pause the protocol 
    // while its being performed.
    //
    // Secondly, co_await is only "allowed" in coroutines.
    // A function is a coroutine is its return type has 
    // some special properties, which task<> has.
    task<> echoClient(std::string message, Socket& sock)
    {

        std::cout << "echo client sending: " << message << std::endl;

        // send the size of the message and 
        // the message itself.
        co_await sock.send(message.size());
        co_await sock.send(message);

        // wait for the server to respond.
        co_await sock.recv(message);
        std::cout << "echo client received: " << message << std::endl;
    }

    // The server receives the message and sends it back.
    task<> echoServer(Socket& sock)
    {
        // receive the size.
        size_t size;
        co_await sock.recv(size);

        std::string message;
        message.resize(size);

        // the size of the received message must match.
        // if not as error will occur.
        co_await sock.recv(message);

        std::cout << "echo server received: " << message << std::endl;

        // send the result back.
        co_await sock.send(message);
    }


    // lets invoke the protocol. There are several ways to 
    // do this but the simplest is to use LocalEvaluator.
    void echoExample()
    {
        std::cout << Color::Green << " ----------- c++20 echoExample ----------- " << std::endl << Color::Default;

        // Get a socket pair which will communicate
        // with each other. This is done in memory.
        auto sockets = LocalAsyncSocket::makePair();

        // coproto is lazy in that when you 
        // construct a protocol nothing actually 
        // happens apart from capturing the 
        // arguments.
        auto server = echoServer(sockets[0]);
        auto client = echoClient("hello world", sockets[1]);

        // to actually execute the protocol,
        // the user must invoke them in some way.
        // multiple protocols can be combined with
        // when_all_ready(...) and then they can be 
        // run with the use of sync_wait(...). This
        // will start the protocols and then block until
        // they complete.
        sync_wait(when_all_ready(std::move(server), std::move(client)));
    }



    ///////////////////////////////////////////////////
    //             Resize example
    //-------------------------------------------------
    // Some buffers can be resized dynamically. This 
    // is determined if the container has a .resize(size)
    // method. See is_resizable_trivial_container for full
    // specification. Examples are std::vector<T>, std::string, etc.
    //
    task<> resizeServer(Socket& sock)
    {
        // no need to sent the size.
        // Containers can be dynamically resized.
        std::string message;
        co_await sock.recvResize(message);

        // or have the container returned.
        auto message2 = co_await sock.recv<std::string>();

        std::cout << "echo server received: " << message << " " << message2 << std::endl;

        // moving the send message in improves efficiency
        // by allowing the protocol proceed immediately
        // while not moving results in blocking until
        // all data has been sent.
        co_await sock.send(std::move(message));
        co_await sock.send(std::move(message2));
    }

    task<> resizeClient(std::string s0, std::string s1, Socket& sock)
    {
        co_await sock.send(s0);
        co_await sock.send(s1);

        co_await sock.recv(s0);
        co_await sock.recv(s1);
    }

    void resizeExample()
    {
        std::cout << Color::Green << " ----------- c++20 resizeExample ----------- " << std::endl << Color::Default;
        auto sockets = LocalAsyncSocket::makePair();


        sync_wait(when_all_ready(
            resizeServer(sockets[0]),
            resizeClient("hello", "world", sockets[1])
        ));
    }



    ///////////////////////////////////////////////////
    //             move send example
    //-------------------------------------------------
    // coproto makes a distinction between calling
    //
    //    sock.send(buffer);
    // 
    // and 
    // 
    //    sock.send(std::move(buffer));
    // 
    // When a buffer is move into send(...), or more generally
    // is an r-value, then the send(...) call does *not block*
    // until all the data das been sent. Instead, the socket
    // takes ownership of the buffer and sends it in the 
    // background. For some protocols this can be important to 
    // pervent deadlocks.
    // 
    // For example, if both parties try to send a message at 
    // the same time. If std::move(...) is not used, then both 
    // parties will (possibly) wait forever for the send to 
    // complete. The send wont complete due to neither party
    // initiating a corresponding recv(...) operation.
    //
    // In this example, both parties will first send and then 
    // receive. 
    task<> moveSendServer(Socket& sock)
    {
        std::vector<char> message(10);
        co_await sock.send(std::move(message));

        // you can also send a copy using the helper function
        // T coproto::copy(const T& t); This will similarly
        // send a copy of message in the background but allows
        // the user to keep a copy of the message.
        message.resize(10);
        co_await sock.send(copy(message));

        message.resize(10);
        co_await sock.recv(message);
        co_await sock.recv(message);
    }

    task<> moveSendClient(Socket& sock)
    {
        std::vector<char> message(10);
        co_await sock.send(std::move(message));

        message.resize(10);
        co_await sock.send(copy(message));

        message.resize(10);
        co_await sock.recv(message);
    }

    void moveSendExample()
    {
        std::cout << Color::Green << " ----------- c++20 move send example ----------- " << std::endl << Color::Default;
        auto sockets = LocalAsyncSocket::makePair();

        sync_wait(when_all_ready(
            moveSendServer(sockets[0]),
            moveSendClient(sockets[1])
        ));
    }


    ///////////////////////////////////////////////////
    //             Error Handling
    //-------------------------------------------------
    // By default coproto communicates errors by throwing
    // an exception. In this example we have a server
    // which throws an error when some event happens.
    // This causes the protocol to unwind at this point. 
    // 
    // LocalExecutor is a bit smart and will cancel the 
    // socket which in turn makes the recv(...) performed
    // by the client to throw an error, std::system_error(
    // code::remoteCancel). 
    //
    // Once both parties throw, LocalExecutor will return
    // an EvalResult which contains the assoicated error.
    task<> errorServer(Socket& sock)
    {
        u64 i = 0;
        while (true)
        {
            bool doThrow;
            co_await sock.recv(doThrow);

            // throw if the client tells us to.
            if (doThrow)
            {
                std::cout << "errorServer throwing at " << i << std::endl;
                sock.close();
                throw std::runtime_error("doThrow");
            }

            co_await sock.send(doThrow);

            ++i;
        }
    }

    task<> errorClient(u64 t, Socket& sock)
    {
        u64 i = 0;
        while (true)
        {
            bool doThrow = i++ == t;
            co_await sock.send(doThrow);

            try
            {
                co_await sock.recv(doThrow);
            }
            catch (std::exception& e)
            {
                std::cout << "recv failed with exception: " << e.what() << std::endl;
                // rethrow the exception
                throw;
            }
        }
    }

    void errorExample()
    {
        std::cout << Color::Green << " ----------- c++20 errorExample ----------- " << std::endl << Color::Default;
        auto sockets = LocalAsyncSocket::makePair();

        auto ec = sync_wait(when_all_ready(
            errorServer(sockets[0]),
            errorClient(6, sockets[1])
        ));

        try {
            // throw if error.
            std::get<0>(ec).result();
            std::get<1>(ec).result();
        }
        catch (std::exception& e)
        {
            std::cout << "error: " << e.what() << std::endl;
        }
    }



    ///////////////////////////////////////////////////
    //             Error Handling with wrap
    // ------------------------------------------------
    // It is possible to ask coproto to catch any 
    // exception for you can convert them to an error_code.
    // This is done by calling calling wrap() on the protocol
    // object. This will change the return to now be a error_code
    // which can be inspected by the caller.
    //
    // Here the server will be the same as before but now the
    // client will call recv(...).wrap() which will return
    // code::remoteCancel, which indicates that the remote
    // party, i.e. server, called cancel on the socket.
    task<> wrapServer(Socket& sock)
    {
        for (u64 i = 0;; ++i)
        {
            bool doThrow;
            co_await sock.recv(doThrow);

            if (doThrow)
            {
                std::cout << "errorServer throwing at " << i << std::endl;
                sock.close();
                throw std::runtime_error("doThrow");
            }

            co_await sock.send(doThrow);
        }
    }

    task<> wrapClient(u64 t, Socket& sock)
    {
        for (u64 i = 0; ; ++i)
        {
            bool doThrow = i == t;
            co_await sock.send(doThrow);

            // here is where we wrap the call, converting
            // any exception to an error_code ec.
            macoro::result<void> result = co_await (sock.recv(doThrow) | macoro::wrap());
            try {
                result.value();
            }
            catch (std::exception& e)
            {
                std::cout << "recv(...) returned ec = " << e.what() << std::endl;

                // we can propagate the error by throwing
                throw e;
            }
        }
    }

    void wrapExample()
    {
        std::cout << Color::Green << " ----------- c++20 wrapExample ----------- " << std::endl << Color::Default;
        auto sockets = LocalAsyncSocket::makePair();

        sync_wait(when_all_ready(
            wrapServer(sockets[0]),
            wrapClient(6, sockets[1])
        ));
    }


    ///////////////////////////////////////////////////
    //             subprotocol example
    // ------------------------------------------------
    // A protocol can call a subprotocol by calling 
    // co_await on the protocol object, just like how
    // a socket operation is performed.
    task<> subprotoServer(Socket& sock)
    {
        std::vector<char> msg(10);
        co_await sock.recv(msg);
        co_await sock.send(msg);

        std::cout << "subprotoServer calling echoClient()" << std::endl;

        std::string str(msg.begin(), msg.end());

        // call into a subprotocol.
        co_await echoClient(str, sock);
    }

    task<> subprotoClient(u64 t, Socket& sock)
    {
        std::vector<char> msg(10);
        for (u64 i = 0; i < msg.size(); ++i)
            msg[i] = 'a' + i;

        co_await sock.send(msg);
        co_await sock.recv(msg);

        std::cout << "subprotoClient calling echoServer()" << std::endl;

        co_await echoServer(sock);
    }

    void subprotoExample()
    {
        std::cout << Color::Green << " ----------- c++20 subprotoExample ----------- " << std::endl << Color::Default;
        auto sockets = LocalAsyncSocket::makePair();

        sync_wait(when_all_ready(
            subprotoServer(sockets[0]),
            subprotoClient(6, sockets[1])
        ));
    }



    ///////////////////////////////////////////////////
    //            async subprotocol example
    // ------------------------------------------------
    // One of they key features of coproto is the ability
    // to run multiple protocols on a single socket. We can 
    // arrange it so that these two protocols are run 
    // concurrently, on a *single thread*. This is achieved
    // by "pausing" the current protocol and running the 
    // other before switching back. In this way we can have
    // both protocols interleave their "round function 
    // messages." 
    // 
    // For example, say our protocol has the following form
    // 
    // Client:                    Server:
    // m1     ---------\                                   |<- Round 1
    // m2     -------\  \                                  | 
    //                \  \----------->   m1                |<- Round 2
    //                 \------------->   m2                | 
    //                     /----------   m3 = f(m1,m2)     | 
    //                    /  /--------   m4 = g(m1,m2,m3)  |    
    // m3     <----------/  /                              |<- Round 3
    // m4     <------------/                               | 
    // m5=h(m3,m4) ----\                                   | 
    // out0 =...        \                                  | 
    //                   \----------->  m5                 |<- Round 4
    //                                  out1 = ...         | 
    //
    // Client sends two messages at the same time, m1,m2. The
    // server receives the both and sends two messages back, 
    // m3,m4. Finally, the client sends a message m5 and then
    // both parties can compute their respective output.
    // 
    // In this case, the protocol has four rounds:
    // 
    // * Round 1: client sends m1,m2.
    // * Round 2: server receives m1,m3 and sends m3,m4
    // * Round 3: client receives m3,m4, sends m5 and outputs out0.
    // * Round 4: server receives m5 and outputs out1.
    // 
    // Now imagine we want to execute this protocol twice
    // on independent inputs. In a naive implementation,
    // you could send/receive *all* the messages (ie m1,
    // ...,m5) for the first protocol and then send all
    // the messages for the second protocol. 
    // 
    // However, this is suboptimal since there is network
    // latency associated with sending a message. A more
    // efficient approach is run Round 1 of both protocols,
    // then Round 2 of both protocol, then Round 3, then 4.
    // 
    // To acheive this, we need to tell coproto when its 
    // ok to suspend the current protocol and go run any
    // other protocols which are ready. 
    // 
    // There are two key concepts in making this work. The
    // first is we can tell coproto that the current protocol
    // can be paused by calling
    // 
    //  co_await EndOfRound{};
    // 
    // This instructs the coproto runtime to check if there
    // are other protocols running, and if so to pause the 
    // current protocol and then go run that one. You should 
    // call co_await EndOfRound{}; when the current protocol
    // is done sending the messages for the current round.
    // 
    // In our example, the client should call co_await EndOfRound{}
    // after sending m2 and m5. The Server should call it
    // after sending m4.
    // 
    // The second concept to understand is if the current 
    // protocol calls recv(...) but the next arriving 
    // message is actually for a different protocol, then
    // the current protocol is paused and the other protocol
    // is resumed. 
    // 
    // Combining these two allow coproto to seamlessly execute
    // both protocols at the same time.
    //
    // To launch more than one protocol at the same time, we use the 
    // task<>::async(...) function. This launched the protocol
    // asynchonously and continues the execution of the current 
    // protocol. When an async protocol is awaited, it will
    // return a Future<T> which can be awaited on to get the
    // return type of the protocol. As we will see later, there
    // are several ways to launch a async protocol. When no
    // parameters are given to task<>::async(), then the protocol
    // is executed on the current thread/strand. Both the current
    // protocol and the async one will be executed on a single 
    // thread but may have their execution interleaved as described
    // above.
    // 
    // This example is implemented here.
    //

    task<> subServer(Socket sock, macoro::thread_pool& tp)
    {

        std::string m1, m2;
        co_await sock.recvResize(m1);
        co_await sock.recvResize(m2);

        // make m3,m4 be the reverse.
        std::string m3(m1.rbegin(), m1.rend());
        std::string m4(m2.rbegin(), m2.rend());

        co_await sock.send(m3);
        co_await sock.send(m4);

        // awaiting on EndOfRound tells the scheduler to
        // pause the current protocol if there are other
        // protocols which are ready. This allows us to control
        // exactly how the rounds of protocols are composed.
        co_await tp.post();

        std::string m5;
        co_await sock.recvResize(m5);

    }


    task<> subClient(Socket sock, macoro::thread_pool& tp)
    {

        std::string m1 = "hello ", m2 = "world";
        co_await sock.send(m1);
        co_await sock.send(m2);

        // awaiting on thread_pool::post() tells the scheduler to
        // "postpones" the current protocol until all other protocols
        // have gotten a chance to run.
        co_await tp.post();

        std::string m3, m4;
        co_await sock.recvResize(m3);
        co_await sock.recvResize(m4);


        std::string m5 = "goodbye";
        co_await sock.send(m5);
    }

    task<> asyncServer(Socket& s, macoro::thread_pool& tp)
    {
        // start two instances of the subprotocol. These
        // will be run in parallel with their rounds being
        // composed together. They return Futures which
        // if we await on will block until the subprotocol 
        // competes
        // 
        // An important note is that we now call fork
        // on the socket. This effectively creates a new
        // socket which can send/receive messages without
        // having the messages getting mixed. 
        macoro::eager_task<void> async0 = subServer(s.fork(), tp) 
            | macoro::make_eager();
        macoro::eager_task<void> async1 = subServer(s.fork(), tp) 
            | macoro::make_eager();

        // Pause this protocol until both subprotocols are done.
        co_await async0;
        co_await async1;
    }

    task<> asyncClient(Socket& s, macoro::thread_pool& tp)
    {
        // start two instances of the subprotocol. These
        // will be run in parallel with their rounds being
        // composed together.
        macoro::eager_task<void> async0 = subClient(s.fork(), tp) | macoro::make_eager();
        macoro::eager_task<void> async1 = subClient(s.fork(), tp) | macoro::make_eager();

        // Pause this protocol until both subprotocols are done.
        co_await async0;
        co_await async1;
    }


    void asyncExample()
    {
        std::cout << Color::Green << " ----------- c++20 asyncExample ----------- " << std::endl << Color::Default;
        auto sockets = LocalAsyncSocket::makePair();
        macoro::thread_pool tp;
        auto work_scope = tp.make_work();
        tp.create_thread();

        sync_wait(when_all_ready(
            asyncServer(sockets[0], tp),
            asyncClient(sockets[1], tp)
        ));
    }

}




void cpp20Tutorial()
{
    echoExample();
    moveSendExample();
    resizeExample();
    errorExample();
    wrapExample();
    subprotoExample();
    asyncExample();
}
#else

void cpp20Tutorial()
{}

#endif
