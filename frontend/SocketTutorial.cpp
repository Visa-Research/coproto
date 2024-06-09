#include "SocketTutorial.h"
#include "coproto/coproto.h"
#include "coproto/Socket/AsioSocket.h"
#include "coproto/Socket/BufferingSocket.h"
#include "tests/Tests.h"
#include <string>

using namespace coproto;

namespace {
	// coproto is designed to directly support
	// a wide variety of networking environments.
	// It is expected that users of the library will 
	// provide their own networking layer which coproto 
	// will integrate with. 
	//
	// For basic protocols coproto can be used with a 
	// blocking socket. However, this imposes certain 
	// limitation and it is suggested that an asynchronous 
	// socket is used.
	// 
	// The library comes with several socket types. This
	// includes
	// 
	// * LocalAsyncSocket - this socket type can only communicate
	//   within the current program. It is intended to be used as 
	//   a method to run unit tests and benchmark protocols. The 
	//   overhead of this socket effectively just involves copying 
	//   the bytes from the sender's buffer to the receiver's buffer.
	//
	// * AsioSocket - this socket type integrates with boost asio's
	//   TCP socket type. It can be used to send data over the 
	//   internet. It requires enabling the optional boost support.
	//  
	// * AsioTlsSocket - this socket integrates with the boost asio 
	//   tls socket type.
	// 
	// * BufferingSocket - this socket type allows the caller to 
	//   buffer the protocol messages and send them in some arbitrary 
	//   way. Example use cases of this socket type is if you want to
	//   send all messages corresponding to one round at the same time.
	// 
	// 
	// All socket types must be wrapped in a coproto::Socket in order
	// to integrate with the library. 
	// 
	// For example, let us assume you have some socket type MySocketType.
	// To integrate with coproto MySocketType must have the following interface:
	// 
	// * SendAwaiter MySocketType::send(span<u8> data, macoro::stop_token token = {})
	//	
	//   Return an arbitrary type `SendAwaiter` that implements the Awaiter concept. 
	//   send(...) takes as input a span<u8> which is the data to be sent. It optionally 
	//   takes a stop_token as input which is used to cancel the async send.
	// 
	//   SendAwaiter should implement the following functions:
	// 
	//     * bool SendAwaiter::await_ready() 
	//       
	//       It is suggested that this always returns false. Return true if the operation 
	//       has somehow already completed and await_suspend should not be called.
	// 
	//     * std::coroutine_handle<> SendAwaiter::await_suspend(std::coroutine_handle<> h) 
	//       
	//       We suggest in this function you start the async send. If the send completes 
	//       immediately/synchronously, then await_suspend should return h. Otherwise, await_suspend
	//       should somehow store h and return std::noop_coroutine(). When the send
	//       completes asynchronously, call h.resume().
	// 
	//		 If the provided token::stop_possible() returns true, a fully conformant implementation
	//		 will register a callback using macoro::stop_callback. This callback will be called
	//       if the user requests the operation to be canceled. Ideally just this operation will
	//       be canceled, however it is also acceptable to cancel all operations. Additionally,
	//       most functionality will work even if stop_token requests are ignored.
	// 
	//     * std::pair<error_code, u64> SendAwaiter::await_resume()
	// 
	//       This function is called after the send has completed to get the result. That is,
	//       its called when await_ready() returns true, or await_suspend(...) returns h, or 
	//       h.resume() is called. This function should return the error_code indicating the 
	//       success or failure of the operation and how many bytes were sent.
	//	
	// * RecvAwaiter MySocketType::recv(span<u8> data, macoro::stop_token token = {})
	// 
	//   This is basically the same as send(...) but should receive data.
	//
	// * void MySocketType::close() or CloseAwaiter MySocketType::close()
	//   
	//   Cancel any outstanding operations. This function might be called more than once.
	//   It should either return void or a awaitable type. In particular, the methods
	// 
	//		* CloseAwaiter::await_ready() -> bool
	//		* CloseAwaiter::await_suspend(std::coroutine_handle<> h) -> { std::coroutine_handle<>, void }
	//		* CloseAwaiter::await_resume() -> void
	// 
	// should exists. await_ready can always return false. await_suspend should return h
	// if close is completed synchronously. Otherwise, store h, return std::noop_coroutine() or void
	// and call h.resume() once completed. await_resume() need not do anything and will be called
	// from h.resume().
	// 
	// A coproto::Socket can be constructed from MySocketType as follows
	// 
	//    MySocketType mySocket = ...
	//    coproto::Socket socket = coproto::makeSocket(std::move(mySocket));
	// 
	// Alternatively,  coproto::Socket has a tagged constructor that you 
	// can use as follows
	// 
	//    MySocketType mySocket = ...
	//    coproto::Socket socket = coproto::Socket(coproto::make_socket_tag{}, std::move(mySocket));
	// 
	// or
	// 
	//    std::unique_ptr<MySocketType> mySocket = ...
	//    coproto::Socket socket = coproto::Socket(coproto::make_socket_tag{}, std::move(mySocket));
	// 
	// This design then allows you to pass around socket which will manage 
	// the lifetime of mySocket. If instead you want coproto::Socket to only
	// reference mySocket, then you can create a wrapper that implements 
	// reference semantics.
	// 
	// A pointer mySocket can be obtained using Socket::getNative().
	//
	// Next we will give a skeleton outline of what MySocketType should look like.  
	//


	// The actual implementation of the socket. It implements
	// the send(...) and recv(...) functions. Both of these 
	// return an Awaiter that tracks & manages the progress of
	// the operation. Additionally we implement the close() function
	// that can be used to cancel any async operation and close
	// the socket.
	//
	struct MySocketType
	{

		////////////////////////////////////////////////
		// Required interface
		////////////////////////////////////////////////


		// This is a coroutine awaiter that is returned by MySocketType::send(...) and MySocketType::recv(...).
		// It represents the asynchronous operation of sending or receiving data. 
		// We also support cancellation of an asynchronous operation. This is achieved 
		// by implementing the coroutine awaiter interface:
		//
		// * await_ready()  
		// * await_suspend()
		// * await_resume() 
		//
		// See the function below for more details.
		struct SendRecvAwaiter
		{
			SendRecvAwaiter(MySocketType* ss, span<u8> dd, bool send, macoro::stop_token&& t);

			// A pointer to the socket that this io operation belongs to.
			MySocketType* mSock;

			// The data to be sent or received. This will shrink as we 
			// make progress in sending or receiving data.
			span<u8> mData;

			enum Type { send, recv };

			// The type of the operation (send or receive).
			Type mType;

			// The coroutine callback that should be called when this operation completes.
			coroutine_handle<> mHandle;

			// An optional token that the user can provide to stop an asynchronous operation.
			// If mToken.stop_possible() returns true, then we will register a callback that
			// is called when/if the user decides to cancel this operation.
			macoro::stop_token mToken;

			// the stop callback.
			macoro::optional_stop_callback mReg;

			// We always return false. This means that await_suspend is always called.
			bool await_ready() { return false; }

			// This function starts the asynchronous operation. It performs something
			// called coroutine symmetric transfer. Thats a fancy way of saying it
			// takes the callback/coroutine_handle h as input. h is a callback to the
			// caller/initiator of the operation. If we complete synchronously, then
			// we should return h. This tells the coroutine framework to resume h, as 
			// the result is ready. If we do not complete synchronously, then we will
			// store h as mHandle and return noop_coroutine(). This is a special handle
			// that tells the caller that there is no more work to do at this time. 
			// Later, when the operation does complete we will resume h/mHandle.
			// 
			// In the event of cancellation, we will have registered a callback (mReg)
			// that is called whenever the user cancels the operation. In this callback
			// we will need to check if the operation is still pending and if so cancel 
			// it and then call h/mHandle
			// 
			// When h/mHandle is resumed, the first thing it does is call await_resume().
			// This allows the initiator (ie h) to get the "return value" of the operation.
			// In this case we will return an error_code and the number of bytes transfered.
			//
			macoro::coroutine_handle<> await_suspend(macoro::coroutine_handle<> h);

#ifdef COPROTO_CPP20
			// this version of await_suspend allows c++20 coroutine support.
			std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) {
				return await_suspend(macoro::coroutine_handle<>(h)).std_cast();
			}
#endif

			// once we complete, this function allows the initiator to get the result
			// of the operation. In particular a pair consisting of an error_code and
			// the number of bytes transfered. On success, the error_code should be
			// 0 and bytes transfered should be all of them. Otherwise error_code 
			// should be some other value.
			std::pair<error_code, u64> await_resume() {
				u64 bytesTransfered = 0; // = ...
				error_code ec; // ...

				// give real implementations for the above.
				throw COPROTO_RTE;

				return { ec, bytesTransfered };
			}

		};


		// cancel all pending operations and prevent any future operations
		// from succeeding.
		void close();

		// This returns an SendRecvAwaiter that will be used to start the async send
		// and manage its execution. As discussed above, SendRecvAwaiter has three 
		// methods: await_ready(), await_suspend(...), await_resume(). These
		// get called in that order. await_suspend will start the actual
		// async operation. await_resume will return the result of the async 
		// operation. send takes as input the data to be sent, and optionally
		// a stop_token. The stop_token allows the user to tell the socket
		// that this operation should be canceled. See SendRecvAwaiter for more details.
		SendRecvAwaiter send(span<u8> data, macoro::stop_token token = {}) { return SendRecvAwaiter(this, data, true, std::move(token)); };

		// This returns an SendRecvAwaiter that will be used to start the async receive
		// and manage its execution. As discussed above, SendRecvAwaiter has three 
		// methods: await_ready(), await_suspend(...), await_resume(). These
		// get called in that order. await_suspend will start the actual
		// async operation. await_resume will return the result of the async 
		// operation. recv takes as input the data to be received, and optionally
		// a stop_token. The stop_token allows the user to tell the socket
		// that this operation should be canceled. See SendRecvAwaiter for more details.
		SendRecvAwaiter recv(span<u8> data, macoro::stop_token token = {}) { return SendRecvAwaiter(this, data, false, std::move(token)); };



	};




	inline MySocketType::SendRecvAwaiter::SendRecvAwaiter(MySocketType* ss, span<u8> dd, bool send, macoro::stop_token&& t)
		: mSock(ss)
		, mData(dd)
		, mType(send ? Type::send : Type::recv)
		, mToken(t)
	{
		COPROTO_ASSERT(dd.size());
	}

	inline void MySocketType::close()
	{
		// call close on the underlaying socket.
	}

	inline coroutine_handle<> MySocketType::SendRecvAwaiter::await_suspend(coroutine_handle<> h)
	{
		// First we need to check it the stop token can be used.
		if (mToken.stop_possible())
		{
			// if so then register the cancellation callback.
			mReg.emplace(mToken, [this] {
				// this will be called if a cancellation has been requested.
				// You should somehow cancel the operation here. Alternatively,
				// if your socket does not support cancellation then you can 
				// just call std::terminate or similar.
				
				// Note that we might call this from within emplace if 
				// a cancellation has already been requested.

				// This might also be called from another thread while the operation 
				// is being started. You must make sure cancellation is thread safe.

				// It is guaranteed that if this function is called then SendRecvAwaiter
				// is still alive. To ensure this, the destructor of SendRecvAwaiter::mReg
				// will block if there is someone in this function.

				// todo: check if we have started the operation and if so cancel it.
				throw COPROTO_RTE;
				});
		}


		// todo: if we have not been canceled, schedule the operation.
		throw COPROTO_RTE;

		bool synchronously = false; // = ...

		if (synchronously)
		{
			// if we complete synchronously, then return h to tell the framework we 
			// can resume the caller.
			return h;
		}
		else
		{
			// save the handle, we will need to call it once the operation does complete...
			mHandle = h;

			// if we dont complete synchronously, then return noop_coroutine 
			// to tell the framework we dont have more work to do.
			return macoro::noop_coroutine();
		}
	}

	void customSocket() {

		std::cout << Color::Green << " ----------- Custom Socket ----------- " << std::endl << Color::Default;

		// Given you socket that implements the send(...),
		// recv(...), close() functions, a coproto::Socket is
		// constructed as follows

		MySocketType mySock; // = ...
		coproto::Socket sock = coproto::makeSocket(mySock);
	}


	void localSocket()
	{
		std::cout << Color::Green << " ----------- Local Socket ----------- " << std::endl << Color::Default;
		auto socketPair = LocalAsyncSocket::makePair();

		coproto::Socket
			sock0 = socketPair[0],
			sock1 = socketPair[1];
	}

	void asioSocket()
	{
#ifdef COPROTO_ENABLE_BOOST
		std::cout << Color::Green << " ----------- Asio Socket ----------- " << std::endl << Color::Default;
		//...
		std::string ip = "127.0.0.1:1212";
		
		// setup boost asio.
		boost::asio::io_context ioc;

		std::vector<std::thread> thrds(4);
		// when w is destroyed the threads will return;
		optional<boost::asio::io_context::work> w(ioc);
		for (auto& t : thrds)
			t = std::thread([&] {ioc.run(); });

		// connect...
		AsioAcceptor connectionAcceptor(ip, ioc);
		AsioConnect connector(ip, ioc);

		auto sockets = macoro::sync_wait(macoro::when_all_ready(connectionAcceptor.accept(), std::move(connector)));

		// in normal code when you only want the server or client, you would do something like
		//     AsioSocket server = macoro::sync_wait(connectionAcceptor.accept());
		// or
		//     AsioSocket client = macoro::sync_wait(std::move(connector));
		AsioSocket
			server = std::get<0>(sockets).result(),
			client = std::get<1>(sockets).result();

		// stop the threads.
		w.reset();

		for (auto& t : thrds)
			t.join();
#endif
	}

	void bufferingSocket()
	{
		std::cout << Color::Green << " ----------- Buffering Socket ----------- " << std::endl << Color::Default;
		BufferingSocket sock;
		bool done = false;

		auto protocol = [&sock,&done]() -> task<> {
			MC_BEGIN(task<>, &sock, &done,
				buffer = std::vector<char>{});

			buffer.resize(42);

			using namespace coproto::internal;


			//enable_if_t<std::is_base_of<I, U>::value>,
			//enable_if_t<std::is_constructible<U, Args...>::value>
			static_assert(coproto::is_poly_emplaceable<SendBuffer, RefSendBuffer, RefSendBuffer&&>::value);

			MC_AWAIT(sock.send(buffer));
			MC_AWAIT(sock.recv(buffer));

			MC_AWAIT(sock.send(buffer));
			MC_AWAIT(sock.recv(buffer));
			done = true;
			MC_END();
		}();

		// start the protocol.
		auto eager = std::move(protocol) | macoro::make_eager();

		while (!done)
		{
			optional<std::vector<u8>> messages = sock.getOutbound();

			if (!messages)
			{
				// The socket was placed in a error state.
				throw COPROTO_RTE;
			}
			else
			{
				// somehow send *messages and receive the incoming messages.

				// in this example we will do something weird and
				// feed the protocols own messages back to itself.
				sock.processInbound(*messages);
			}
		}
	}

}

void SocketTutorial()
{
	customSocket();

	localSocket();

	asioSocket();

	bufferingSocket();

}
