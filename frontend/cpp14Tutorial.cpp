#include "cpp14Tutorial.h"
#include "cpp20Tutorial.h"
#include "coproto/coproto.h"
#include "tests/Tests.h"
#include <string>
#include "macoro/wrap.h"


#include "macoro/thread_pool.h"


using namespace coproto;
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
    // need to call MC_AWAIT on each socket operation. 
    // This gives the coproto framework an opportunity
    // to start the IO operations and pause the protocol 
    // while its being performed.
    // 
    // Secondly, CP_BEGIN, MC_AWAIT,... is only "allowed" in coroutines.
    // A function is a coroutine is its return type has 
    // some special properties, which Proto has.
    task<> echoClient(std::string message, Socket& socket)
    {
        // first we have to start with the MC_BEGIN(task<>,);
        // macro. We will discuss what this does later. The
        // parameter(s) are a lambda capture of the function 
        // parameter. Only these parameters can be used inside
        // the "coroutine".
        //
        // Here we are "lambda capturing" message by copy
        // and socket by reference. 
        MC_BEGIN(task<>,message, &socket);
        std::cout << "echo client sending: " << message << std::endl;

        // send the size of the message and the message 
        // itself. Instead of using the c++20 co_await keyword
        // we will use the MC_AWAIT macro to achieve a similar
        // result. This will pause the function at this point
        // until the operation has completed.
        MC_AWAIT(socket.send(message.size()));
        MC_AWAIT(socket.send(message));

        // wait for the server to respond.
        MC_AWAIT(socket.recv(message));
        std::cout << "echo client received: " << message << std::endl;

        // finally, we end the coroutine with MC_END.
        MC_END();
    }


    task<> echoServer(Socket& s)
    {
        // again, the parameters to CP_BEGIN represent
        // the lambda capture for this coroutine. All
        // local variables must be declared here.
        //
        // we are again capturing socket by reference.
        // Additionally, we make two local variables,
        // size ith type size_t, and message with type std::string.
        MC_BEGIN(task<>,
            &socket = s,
            size = size_t{},
            message = std::string{}
        );

        MC_AWAIT(socket.recv(size));
        message.resize(size);

        // the size of the received message must match.
        // if not as error will occur.
        MC_AWAIT(socket.recv(message));

        std::cout << "echo server received: " << message << std::endl;

        // send the result back.
        MC_AWAIT(socket.send(message));

        MC_END();

    }

    void echoExample()
    {
        std::cout << Color::Green << " ----------- c++14 echoExample ----------- " << std::endl << Color::Default;
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
    //          Macro Explanation            
    // -------------------------------------------------
    // Here we explain what the macros are doing. While not
    // strictly necessary to understand, it can be helpful.
    // 
    // The idea of a coroutine is a function that can be 
    // paused and later resumed where it was left off. 
    // c++ 20 has language support for this but a similar
    // functionality can be obtained manually.
    // 
    // We first construct an "inner function" which we 
    // implement with a lambda. This lambda takes as input
    // the "coroutine" context which tell the lambda where
    // to resume the function at. The lambda then jumps
    // to this location using a switch statement. If the 
    // coroutine was previously paused due to calling MC_AWAIT,
    // the result of the MC_AWAIT is obtained and then the 
    // coroutine is continued. 
    // 
    // Finally, we use the function makeLambdatask<> to convert
    // the lambda into a task<> object. More details below.
//    task<> macroClient(std::string message, Socket& socket)
//    {
//        // ================ CP_BEGIN =============
//        // MC_BEGIN(task<>,message, &socket); gets expanded to the following.
//
//        // construct a task<> which is based on a lambda
//        return makeLambdatask<>(
//
//            // capture the parameters by value or reference.
//            [message, &socket] (CoprotoContext<void>* _coproto_context_) mutable ->void
//            {
//
//                // use a switch statement to jump to the current position 
//                // of the coroutine. On the first call this will be "case 0".
//                switch (_coproto_context_->getCoroutineResumeIdx())
//                {
//                case ResumeIndex::Start:
//
//                    // The main thing to understand is that the body of the function
//                    // is actually a nested lambda with the function parameters
//                    // being captured by the lambda. This lambda will be repeatedly
//                    // called and the first operation is does is jump back to where it 
//                    // last left off via the switch statement. Due to jumping/switch, 
//                    // local variables basically must all be placed in the lambda 
//                    // capture. 
//                    // 
//                    // ========= End of CP_BEGIN =============
//
//
//
//                    // ============ MC_AWAIT ===============
//                    // the statement MC_AWAIT(socket.send(message.size()));
//                    // effectively expands to the following.
//
//#define STATEMENT   socket.send(message.size())
//                {
//                    using Awaiter = std::remove_reference_t<decltype(_coproto_context_->constructAwaiter(STATEMENT))>;
//                    {
//                        Awaiter& awaiter = _coproto_context_->constructAwaiter(STATEMENT);
//                        if (!awaiter.await_ready())
//                        {
//                            _coproto_context_->setCoroutineResumeIdx((ResumeIndex)1);
//                            return;
//                        }
//                    }
//                case (ResumeIndex)1:
//                    Awaiter & awaiter = _coproto_context_->getAwaiter<Awaiter>();
//                    awaiter.await_resume();
//                }
//
//                // The first thing to understand is that the argument to
//                // MC_AWAIT is using to create an "Awaiter". This object
//                // knows how to start a "task<>" and will tell us if it
//                // is immediately ready. It is constructed by calling 
//                // 
//                //		Awaiter a = task<>PromiseBase::await_transform(STATEMENT);
//                // 
//                // When await_ready() is called, the protocol for STATEMENT
//                // is started. If the protocol finishes immediately, then true
//                // is returned and we continue the coroutine. 
//                // 
//                // If the underlying protocol suspends, then false is returned 
//                // and we need to suspend this coroutine. This is done by 
//                // recording our current "program pointer" via setCoroutineResumeIdx(...).
//                // The value passed in corresponds to the switch case the follows,
//                // in this example it is "case 1". Later, this lambda will be called
//                // again and the switch statement will jump us back to just after
//                // the return.
//                // 
//                // In either case, we get the Awaiter and call await_resume().
//                // If the Awaiter/STATEMENT had a return value, then await_resume()
//                // would return it. If there was some error, then await_resume()
//                // will throw it.
//                //
//                // The above is almost identical to what the c++20 coroutine 
//                // machinery does automatically.
//                //
//
//
//                // ========= End of MC_AWAIT =============
//
//                std::cout << "macro client sending: " << message << std::endl;
//
//                // we will not expand the next calls since they are the same.
//                MC_AWAIT(socket.send(message));
//                MC_AWAIT(socket.recv(message));
//
//
//                // finally, the MC_END(); closes the switch statement.
//                break;
//                default:
//                    break;
//                }
//            });
//    }
//
//
//    void macroExample()
//    {
//        std::cout << Color::Green << " ----------- c++14 macroExample  ----------- " << std::endl << Color::Default;
//        LocalEvaluator eval;
//        auto sockets = eval.getSocketPair();
//
//        auto server = echoServer(sockets[0]);
//        auto client = macroClient("hello world", sockets[1]);
//
//        eval.eval(server, client);
//    }


    ///////////////////////////////////////////////////
    //             Resize example
    //-------------------------------------------------
    // Some buffers can be resized dynamically. This 
    // is determined if the container has a .resize(size)
    // method. See is_resizable_trivial_container for full
    // specification. Examples are std::vector<T>, std::string, etc.

    task<> resizeServer(Socket& sock)
    {
        // capture any local variables.
        MC_BEGIN(task<>,
            &sock,
            message = std::string{},
            message2 = std::string{});

        // no need to sent the size.
        // Containers can be dynamical resized.
        MC_AWAIT(sock.recvResize(message));

        // or have the container returned.
        MC_AWAIT_SET(message2, sock.recv<std::string>());

        std::cout << "echo server received: " << message << " " << message2 << std::endl;

        // moving the send message in improves efficiency
        // by allowing the protocol proceed immediately
        // while not moving results in blocking until
        // all data has been sent.
        MC_AWAIT(sock.send(std::move(message)));
        MC_AWAIT(sock.send(std::move(message2)));

        MC_END();
    }

    task<> resizeClient(std::string s0, std::string s1, Socket& sock)
    {
        MC_BEGIN(task<>,s0, s1, &sock);
        MC_AWAIT(sock.send(s0));
        MC_AWAIT(sock.send(s1));
        MC_AWAIT(sock.recv(s0));
        MC_AWAIT(sock.recv(s1));
        MC_END();
    }

    void resizeExample()
    {
        std::cout << Color::Green << " ----------- c++14 resizeExample ----------- " << std::endl << Color::Default;
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
    // until all the data has been sent. Instead, the socket
    // takes ownership of the buffer and sends it in the 
    // background. For some protocols this can be important to 
    // prevent deadlocks.
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
        MC_BEGIN(task<>,&sock,
            message = std::vector<char> (10)
            );
        MC_AWAIT(sock.send(std::move(message)));

        message.resize(10);
        MC_AWAIT(sock.recv(message));

        MC_END();
    }

    task<> moveSendClient(Socket& sock)
    {
        MC_BEGIN(task<>,&sock,
            message = std::vector<char>(10)
        );
        MC_AWAIT(sock.send(std::move(message)));

        message.resize(10);
        MC_AWAIT(sock.recv(message));

        MC_END();
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
    // an EvalResult which contains the associated error.
    task<> errorServer(Socket& sock)
    {
        MC_BEGIN(task<>,i = u64{}, doThrow = bool{}, &sock);
        while (true)
        {
            MC_AWAIT(sock.recv(doThrow));

            // throw if the client tells us to.
            if (doThrow)
            {
                std::cout << "errorServer throwing at " << i << std::endl;
                MC_AWAIT(sock.close());
                throw std::runtime_error("doThrow");
            }

            MC_AWAIT(sock.send(doThrow));

            ++i;
        }

        MC_END();
    }

    task<> errorClient(u64 t, Socket& sock)
    {
        MC_BEGIN(task<>,i = u64{}, doThrow = bool{}, &sock, t);
        while (true)
        {
            doThrow = i++ == t;
            MC_AWAIT(sock.send(doThrow));

            // will throw once the server throws.
            MC_AWAIT(sock.recv(doThrow));

        }

        MC_END();
    }

    void errorExample()
    {
        std::cout << Color::Green << " ----------- c++14 errorExample ----------- " << std::endl << Color::Default;
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
    // This is done by calling wrap() on the protocol
    // object. This will change the return to now be a error_code
    // which can be inspected by the caller.
    //
    // Here the server will be the same as before but now the
    // client will call recv(...).wrap() which will return
    // code::remoteCancel, which indicates that the remote
    // party, i.e. server, called cancel on the socket.
    task<> wrapServer(Socket& sock)
    {
        MC_BEGIN(task<>,i = u64{}, doThrow = bool{}, &sock);
        for (i = 0;; ++i)
        {
            MC_AWAIT(sock.recv(doThrow));

            if (doThrow)
            {
                std::cout << "errorServer throwing at " << i << std::endl;
                MC_AWAIT(sock.close());
                throw std::runtime_error("doThrow");
            }

            MC_AWAIT(sock.send(doThrow));
        }
        MC_END();
    }

    task<> wrapClient(u64 t, Socket& sock)
    {
        MC_BEGIN(task<>,
            i = u64{},
            doThrow = bool{},
            ec = macoro::result<void>{},
            &sock, t);

        for (i = 0; ; ++i)
        {
            doThrow = i == t;
            MC_AWAIT(sock.send(doThrow));

            // here is where we wrap the call, converting
            // any exception to an error_code ec.
            MC_AWAIT_SET(ec, sock.recv(doThrow) | macoro::wrap());
            try {
                ec.value();
            }
            catch(std::exception& e)
            {
                std::cout << "recv(...) returned ec = " << e.what() << std::endl;

                // we can propagate the error by throwing
                throw e;
            }
        }
        MC_END();
    }

    void wrapExample()
    {
        std::cout << Color::Green << " ----------- c++14 wrapExample ----------- " << std::endl << Color::Default;
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
    // MC_AWAIT on the protocol object, just like how
    // a socket operation is performed.
    task<> subprotoServer(Socket sock)
    {
        MC_BEGIN(task<>,msg = std::vector<char>(10), str = std::string{}, sock);
        MC_AWAIT(sock.recv(msg));
        MC_AWAIT(sock.send(msg));

        std::cout << "subprotoServer calling echoClient()" << std::endl;

        str.insert(str.end(), msg.begin(), msg.end());

        // call into a subprotocol.
        MC_AWAIT(echoClient(str, sock));
        MC_END();
    }

    task<> subprotoClient(u64 t, Socket sock)
    {
        std::vector<char> msg(10);
        for (u64 i = 0; i < msg.size(); ++i)
            msg[i] = 'a' + i;

        MC_BEGIN(task<>,msg, t, sock);

        MC_AWAIT(sock.send(msg));
        MC_AWAIT(sock.recv(msg));

        std::cout << "subprotoClient calling echoServer()" << std::endl;

        MC_AWAIT(echoServer(sock));

        MC_END();
    }

    void subprotoExample()
    {
        std::cout << Color::Green << " ----------- c++14 subprotoExample ----------- " << std::endl << Color::Default;
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
    // To achieve this, we need to tell coproto when its 
    // ok to suspend the current protocol and go run any
    // other protocols which are ready. 
    // 
    // There are two key concepts in making this work. The
    // first is we can tell coproto that the current protocol
    // can be paused by calling
    // 
    //  MC_AWAIT(EndOfRound{});
    // 
    // This instructs the coproto runtime to check if there
    // are other protocols running, and if so to pause the 
    // current protocol and then go run that one. You should 
    // call MC_AWAIT(EndOfRound{}); when the current protocol
    // is done sending the messages for the current round.
    // 
    // In our example, the client should call MC_AWAIT(EndOfRound{})
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
    // asynchronously and continues the execution of the current 
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

    task<> subServer(Socket sock, macoro::thread_pool& tp)
    {

        MC_BEGIN(task<>,
            m1 = std::string{},
            m2 = std::string{},
            m3 = std::string{},
            m4 = std::string{},
            m5 = std::string{},
            sock,
            &tp
        );

        MC_AWAIT(sock.recvResize(m1));
        MC_AWAIT(sock.recvResize(m2));

        // make m3,m4 be the reverse.
        m3.insert(m3.end(), m1.rbegin(), m1.rend());
        m4.insert(m4.end(), m2.rbegin(), m2.rend());

        MC_AWAIT(sock.send(m3));
        MC_AWAIT(sock.send(m4));

        // awaiting on thread_pool::post() tells the scheduler to
        // "postpones" the current protocol until all other protocols
        // have gotten a chance to run.
        MC_AWAIT(tp.post());

        MC_AWAIT(sock.recvResize(m5));
        MC_END();
    }


    task<> subClient(Socket sock, macoro::thread_pool& tp)
    {

        std::string m1 = "hello ", m2 = "world";
        MC_BEGIN(task<>,m1, m2, sock,
            m3 = std::string{},
            m4 = std::string{},
            m5 = std::string{},
            &tp
        );
        MC_AWAIT(sock.send(m1));
        MC_AWAIT(sock.send(m2));

        // awaiting on thread_pool::post() tells the scheduler to
        // "postpones" the current protocol until all other protocols
        // have gotten a chance to run.
        MC_AWAIT(tp.post());

        MC_AWAIT(sock.recvResize(m3));
        MC_AWAIT(sock.recvResize(m4));


        m5 = "goodbye";
        MC_AWAIT(sock.send(m5));

        MC_END();
    }

    task<> asyncServer(Socket& s, macoro::thread_pool& tp)
    {
        MC_BEGIN(task<>,
            async0 = macoro::eager_task<void>{},
            async1 = macoro::eager_task<void>{},
            &s,
            &tp
        );
        // start two instances of the subprotocol. These
        // will be run in parallel with their rounds being
        // composed together. They return Futures which
        // if we await on will block until the subprotocol 
        // competes. 
        // 
        // An important note is that we now call fork
        // on the socket. This effectively creates a new
        // socket which can send/receive messages without
        // having the messages getting mixed. 
        async0 = subServer(s.fork(), tp) | macoro::make_eager();
        async1 = subServer(s.fork(), tp) | macoro::make_eager();

        // Pause this protocol until both subprotocols are done.
        MC_AWAIT(async0);
        MC_AWAIT(async1);

        MC_END();
    }

    task<> asyncClient(Socket& s, macoro::thread_pool& tp)
    {
        MC_BEGIN(task<>,
            async0 = macoro::eager_task<void>{},
            async1 = macoro::eager_task<void>{},
            &s,
            &tp
        );

        // start two instances of the subprotocol. These
        // will be run in parallel with their rounds being
        // composed together.
       async0 = subClient(s.fork(), tp) | macoro::make_eager();
       async1 = subClient(s.fork(), tp) | macoro::make_eager();

        // Pause this protocol until both subprotocols are done.
        MC_AWAIT(async0);
        MC_AWAIT(async1);
        MC_END();
    }


    void asyncExample()
    {
        std::cout << Color::Green << " ----------- c++14 asyncExample ----------- " << std::endl << Color::Default;
        auto sockets = LocalAsyncSocket::makePair();
        macoro::thread_pool tp;
        auto work_scope = tp.make_work();
        tp.create_thread();

        sync_wait(when_all_ready(
            asyncServer(sockets[0], tp),
            asyncClient(sockets[1], tp)
        ));
    }


    task<> executorServer(Socket& s)
    {
        MC_BEGIN(task<>,
            async0 = macoro::eager_task<void>{},
            async1 = macoro::eager_task<void>{},
            &s,
            tp0 = macoro::thread_pool{},
            tp1 = macoro::thread_pool{},
            w0 = std::move(macoro::thread_pool::work{}),
            w1 = std::move(macoro::thread_pool::work{}),
            s0 = Socket{},
            s1 = Socket{}
        );
        // In this example we show how one can set a default
        // executor for a socket and/or fork. 

        // for example, we can create two forks.
        s0 = s.fork();
        s1 = s.fork();

        // each fork can be assigned its own executor.
        // When an async operation completes, its will be resumed
        // on the given executor. You can provide your own executor,
        // it just needs to have a member function
        //
        // void schedule(macoro::coroutine_handle<void>);
        //
        // see the declaration of Socket::setExecutor for details.
        s0.setExecutor(tp0);
        s1.setExecutor(tp1);


        // create the threads for the execution contexts. This is done by
        // creating "fake" work objects and then creating the threads. Once 
        // threads run out of work and the work objects are destroyed, the 
        // threads will terminate.
        w0 = tp0.make_work();
        w1 = tp1.make_work();
        tp0.create_thread();
        tp1.create_thread();

        // we can now eagerly start both protocols. This differs from the previous
        // example in that we dont need to constantly post() to our chosen executor
        // after each socket operation. Instead the socket will do this for us.
        async0 = subprotoServer(s0) | macoro::make_eager();
        async1 = subprotoClient(1, s1) | macoro::make_eager();

        // Pause this protocol until both subprotocols are done.
        MC_AWAIT(async0);
        MC_AWAIT(async1);

        MC_AWAIT(s0.flush());
        MC_AWAIT(s1.flush());

        // let the threads join.
        w0 = {};
        w1 = {};

        MC_END();
    }

    task<> executorClient(Socket& s)
    {
        MC_BEGIN(task<>,
            async0 = macoro::eager_task<void>{},
            async1 = macoro::eager_task<void>{},
            &s
        );

        // For the client we will run everything on the default execution context.
        async0 = subprotoClient(1, s.fork()) | macoro::make_eager();
        async1 = subprotoServer(s.fork()) | macoro::make_eager();

        // Pause this protocol until both subprotocols are done.
        MC_AWAIT(async0);
        MC_AWAIT(async1);
        MC_END();
    }


    void executorExample()
    {

        std::cout << Color::Green << " ----------- c++14 executorExample ----------- " << std::endl << Color::Default;
        auto sockets = LocalAsyncSocket::makePair();


        sync_wait(when_all_ready(
            executorServer(sockets[0]),
            executorClient(sockets[1])
        ));
    }

}

void cpp14Tutorial()
{
    echoExample();
    //macroExample();
    resizeExample();
    errorExample();
    wrapExample();
    subprotoExample();
    asyncExample();
    executorExample();

}

