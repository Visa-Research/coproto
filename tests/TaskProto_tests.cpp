
#include "TaskProto_tests.h"
#include "coproto/Socket/Socket.h"
#include "coproto/Socket/LocalAsyncSock.h"
#include "macoro/task.h"
#include "macoro/stop.h"
#include "macoro/sync_wait.h"
#include "macoro/when_all.h"
#include "macoro/wrap.h"
#include "macoro/start_on.h"
#include <numeric>
#include "eval.h"
#include "macoro/when_all_scope.h"

namespace coproto
{

#define TEST(X) if(!(X)) throw COPROTO_RTE

	namespace tests
	{
		namespace {
			std::mutex gPrntMtx;

			auto types = {
				EvalTypes::async,
				EvalTypes::Buffering//,
				//EvalTypes::blocking
			};
		}

#ifdef COPROTO_CPP20

		void task_proto_test()
		{
			for (auto type : types)//{EvalTypes::blocking})
			{

				//int recv = 0;
				//std::array<bool, 2> started = { false,false };

				auto task_proto = [&](Socket ss, bool send)->task<void>
					{

						int cc;
						if (send)
						{
							//started[0] = true;
							cc = 42;
							co_await ss.send(cc);

						}
						else
						{
							//started[1] = true;
							co_await ss.recv(cc);
							//recv = cc;
						}
					};

				//auto tt = {{ task_proto(Socket{}, 0), task_proto(Socket{}, 0) }}
				//    | macoro::when_all_ready()
				//    | macoro::make_blocking();
				auto r = eval(task_proto, type);

				std::get<0>(r).result();
				std::get<1>(r).result();
			}
		}


		void task_strSendRecv_Test()
		{
			//int sDone = 0, rDone = 0;
			auto proto = [&](Socket& s, bool party) -> task<void> {

				bool verbose = false;
				std::string sStr("hello from 0");
				std::string sRtr; sRtr.resize(sStr.size());
				for (u64 i = 0; i < 5; ++i)
				{
					if (party)
					{
						co_await s.send(sStr);

						if (verbose)
							std::cout << "s send " << std::endl;
						//co_await EndOfRound();
						if (verbose)
							std::cout << "s eor " << std::endl;

						sStr = co_await s.recv<std::string>();
						if (verbose)
							std::cout << "s recv " << std::endl;

						if (sStr != "hello from " + std::to_string(i * 2 + 1))
							throw std::runtime_error(COPROTO_LOCATION);
						sStr.back() += 1;
					}
					else
					{
						co_await s.recv(sRtr);
						if (verbose)
							std::cout << "r recv " << std::endl;

						if (sRtr != "hello from " + std::to_string(i * 2 + 0))
							throw std::runtime_error(COPROTO_LOCATION);

						sRtr.back() += 1;
						co_await s.send(sRtr);
						if (verbose)
							std::cout << "r send " << std::endl;
						//co_await EndOfRound();
						if (verbose)
							std::cout << "r eor " << std::endl;

					}
				}
				if (party)
				{
					if (verbose)
						std::cout << "s done " << std::endl;
					//++sDone;
				}
				else
				{
					if (verbose)
						std::cout << "r done " << std::endl;
					//++rDone;
				}

				};



			for (auto t : types)
			{
				auto r = eval(proto, t);
				std::get<0>(r).result();
				std::get<1>(r).result();
			}

		}




		template<typename awaitable>
		struct wrapped_awaitable
		{
			using traits = macoro::awaitable_traits<awaitable>;
			using awaitable_storage = macoro::remove_rvalue_reference_t<awaitable>;
			using awaiter_storage = macoro::remove_rvalue_reference_t<typename traits::awaiter>;
			using awaitar_result = macoro::remove_rvalue_reference_t<typename traits::await_result>;

			struct awaiter
			{
				awaiter_storage m_awaiter;
				std::source_location m_loc;
				bool await_ready(std::source_location loc = std::source_location::current())
				{
					m_loc = loc;
					return m_awaiter.await_ready();
				}

				template<typename promise>
				auto await_suspend(std::coroutine_handle<promise> h)
				{
					if constexpr (requires(awaiter_storage m_awaiter) { { m_awaiter.await_suspend(h, m_loc) } -> std::convertible_to<int>; })
					{
						return m_awaiter.await_suspend(h, m_loc);
					}
					else
					{
						return m_awaiter.await_suspend(h);
					}
				}

				//template<typename promise>
				//auto await_suspend(coroutine_handle<promise> h, std::source_location loc = std::source_location::current())
				//{
				//	if constexpr (requires(awaiter_storage m_awaiter) { { m_awaiter.await_suspend(h, loc) } -> std::convertible_to<int>; })
				//	{
				//		return m_awaiter.await_suspend(h, loc);
				//	}
				//	else
				//	{
				//		return m_awaiter.await_suspend(h);
				//	}
				//}

				macoro::result<awaitar_result> await_resume() noexcept
				{
					try
					{
						if constexpr (std::is_same_v<awaitar_result, void>)
						{
							m_awaiter.await_resume();
							return macoro::Ok();
						}
						else
						{
							return macoro::Ok(m_awaiter.await_resume());
						}
					}
					catch (...)
					{
						return macoro::Err(std::current_exception());
					}
				}
			};

			awaitable_storage m_awaitable;

			awaiter operator co_await()&
			{
				return awaiter{ macoro::get_awaiter(m_awaitable) };
			}

			awaiter operator co_await()&&
			{
				return awaiter{ macoro::get_awaiter(std::forward<awaitable>(m_awaitable)) };
			}
		};

		template<typename awaitable>
		wrapped_awaitable<awaitable> wrap2(awaitable&& a) {
			return { std::forward<awaitable>(a) };
		}


		void task_resultSendRecv_Test()
		{

			auto proto = [](Socket& sock, bool party) -> task<void> {
				std::string str("hello from 0");
				//co_await Name("main");

				for (u64 i = 0; i < 5; ++i)
				{
					if (party)
					{
						//std::cout << " p1 sent " << i << std::endl;
						auto ec = co_await macoro::wrap(sock.send(str));
						//std::cout << " p1 sent " << i << " ok" << std::endl;

						////std::cout << " p1 recv " << i << std::endl;
						auto r = co_await(sock.recv<std::string>() | macoro::wrap());
						//std::cout << " p1 recv " << i << " ok " << std::endl;

						if (r.has_error())
							throw std::runtime_error(COPROTO_LOCATION);

						str = r.value();

						if (str != "hello from " + std::to_string(i * 2 + 1))
							throw std::runtime_error(COPROTO_LOCATION);
						str.back() += 1;
					}
					else
					{
						//std::cout << " p0 recv " << i << std::endl;
						co_await sock.recv(str);
						//std::cout << " p0 recv " << i << " ok" << std::endl;

						if (str != "hello from " + std::to_string(i * 2 + 0))
							throw std::runtime_error(COPROTO_LOCATION);

						str.back() += 1;

						//std::cout << " p0 sent " << i << std::endl;
						co_await sock.send(str);
						//std::cout << " p0 sent " << i << " ok" << std::endl;

					}
				}
				};


			for (auto t : types)
			{
				auto r = eval(proto, t);
				std::get<0>(r).result();
				std::get<1>(r).result();
			}
		}

		void task_typedRecv_Test()
		{
			auto proto = [](Socket& sock, bool party) -> task<void> {
				bool verbose = false;

				std::vector<u64> buff, rBuff;
				for (u64 i = 0; i < 5; ++i)
				{
					if (party)
					{
						buff.resize(1 + i * 2);
						std::fill(buff.begin(), buff.end(), i * 2);
						if (verbose)std::cout << "send0 b " << i << std::endl;
						co_await sock.send(std::move(buff));
						if (verbose)std::cout << "send0 e " << i << std::endl;
						if (verbose)std::cout << "recv1 b " << i << std::endl;
						rBuff = co_await sock.recv<std::vector<u64>>();
						if (verbose)std::cout << "recv1 e " << i << std::endl;

						buff.resize(2 + i * 2);
						std::fill(buff.begin(), buff.end(), i * 2 + 1);

						if (buff != rBuff)
							throw std::runtime_error(COPROTO_LOCATION);
					}
					else
					{
						if (verbose)std::cout << "recv0 b " << i << std::endl;
						rBuff = co_await sock.recv<std::vector<u64>>();
						if (verbose)std::cout << "recv0 e " << i << std::endl;
						buff.resize(1 + i * 2);
						std::fill(buff.begin(), buff.end(), i * 2);

						if (buff != rBuff)
							throw std::runtime_error(COPROTO_LOCATION);

						buff.resize(buff.size() + 1);
						std::fill(buff.begin(), buff.end(), i * 2 + 1);
						if (verbose)std::cout << "send1 b " << i << std::endl;
						co_await sock.send(std::move(buff));
						if (verbose)std::cout << "send1 e " << i << std::endl;

					}
				}
				};


			for (auto t : types)
			{
				eval(proto, t);
			}

		}
		void task_zeroSendRecv_Test()
		{

			auto proto = [](Socket& s, bool party) -> task<void> {

				std::vector<u64> buff;
				co_await s.send(buff);
				};


			for (auto t : types)
			{
				auto r = eval(proto, t);

				try {
					std::get<0>(r).result();
				}
				catch (std::system_error& e)
				{
					if (e.code() != code::sendLengthZeroMsg)
						throw std::runtime_error("");
				}

				try {
					std::get<1>(r).result();
				}
				catch (std::system_error& e)
				{
					if (e.code() != code::sendLengthZeroMsg)
						throw std::runtime_error("");
				}
			}
		}
		void task_zeroSendRecv_ErrorCode_Test()
		{

			auto proto = [](Socket& s, bool party) -> task<void> {

				std::vector<u64> buff;
				auto ec = co_await(s.send(buff) | macoro::wrap());

				if (as_error_code(ec.error()) != code::sendLengthZeroMsg)
					throw std::runtime_error("");

				};
			for (auto t : types)
			{
				auto r = eval(proto, t);
				std::get<0>(r).result();
				std::get<1>(r).result();
			}
		}

		//static thread_local 
	//#define MACORO_TRY std::exception_ptr macoro_ePtr; try
	//#define MACORO_CATCH(NAME)  catch(...) { macoro_ePtr = std::current_exception(); } if(auto NAME = std::exchange(macoro_ePtr, nullptr))  

		void task_badRecvSize_Test()
		{
			auto proto = [](Socket& s, bool party) -> task<void> {

				MACORO_TRY{

					std::vector<u64> buff(3);

					if (party)
					{
						co_await s.send(buff);
					}
					else
					{
						buff.resize(1);
						co_await s.recv(buff);
					}
				}
					MACORO_CATCH(ePtr)
				{
					if (!s.closed()) co_await s.close();
					std::rethrow_exception(ePtr);
				}
				};


			for (auto t : types)
			{
				auto r = eval(proto, t);


				try {
					std::get<0>(r).result();
					throw std::runtime_error(COPROTO_LOCATION);
				}
				catch (BadReceiveBufferSize& b) {
					if (b.mBufferSize != 8 || b.mReceivedSize != 24)
						throw;
				};
				try {
					std::get<1>(r).result();

					if (t != EvalTypes::Buffering)
						throw std::runtime_error(COPROTO_LOCATION);
				}
				catch (std::system_error& e)
				{
					if (e.code() != code::remoteClosed)
						throw;
				}
			}
		}

		void task_badRecvSize_ErrorCode_Test()
		{

			for (auto t : types)
			{
				auto proto = [t](Socket& s, bool party) -> task<void> {
					auto buff = std::vector<u64>(3);
					auto r = macoro::result<void>{};

					if (party)
					{
						r = co_await(s.send(buff) | macoro::wrap());

						if (t != EvalTypes::Buffering)
							if (as_error_code(r.error()) != code::remoteClosed)
								throw std::runtime_error("");

						r = co_await(s.send(buff) | macoro::wrap())
							;
						if (t != EvalTypes::Buffering)
						{
							auto ec = as_error_code(r.error());
							if (ec != code::cancel)
								throw std::runtime_error("");
						}

					}
					else
					{
						buff.resize(1);
						r = co_await(s.recv(buff) | macoro::wrap());

						if (as_error_code(r.error()) != code::badBufferSize)
							throw std::runtime_error("");


						r = co_await(macoro::wrap(s.recv(buff)));
						if (as_error_code(r.error()) != code::cancel)
							throw std::runtime_error("");

						r = co_await(macoro::wrap(s.send(buff)));
						if (as_error_code(r.error()) != code::cancel)
							throw std::runtime_error("");
					}
					};


				auto r = eval(proto, t);
				std::get<0>(r).result();
				std::get<1>(r).result();
			}
			//auto proto = [](Socket& s, bool party) -> task<void> {

			//	std::vector<u64> buff(3);

			//	if (party)
			//	{
			//		auto r = co_await(s.send(buff) | macoro::wrap());
			//		if (as_error_code(r.error()) != code::remoteClosed)
			//			throw std::runtime_error("");

			//		r = co_await(s.send(buff) | macoro::wrap());
			//		if (as_error_code(r.error()) != code::remoteClosed)
			//			throw std::runtime_error("");

			//	}
			//	else
			//	{
			//		buff.resize(1);
			//		auto r = co_await(s.recv(buff) | macoro::wrap());

			//		if (as_error_code(r.error()) != code::badBufferSize)
			//			throw std::runtime_error("");


			//		r = co_await macoro::wrap(s.recv(buff));
			//		if (as_error_code(r.error()) != code::cancel)
			//			throw std::runtime_error("");

			//		r = co_await macoro::wrap(s.send(buff));
			//		if (as_error_code(r.error()) != code::cancel)
			//			throw std::runtime_error("");
			//	}
			//};


			//for (auto t : types)
			//{
			//	auto r = eval(proto, t);
			//	std::get<0>(r).result();
			//	std::get<1>(r).result();
			//}
		}

		void task_moveSend_Test()
		{

			for (auto t : types)
			{
				auto proto = [](Socket& s, bool party) -> task<void>
					{
						int i = 0;
						std::vector<int> v(10);

						co_await s.send(std::move(i));
						co_await s.recv(i);

						co_await s.send(std::move(v));
						co_await s.recvResize(v);

					};

				std::thread thrd;

				auto r = eval(proto, t);

				std::get<0>(r).result();
				std::get<1>(r).result();
			}
		}

		void task_throws_Test()
		{

			auto proto = [](Socket& s, bool party) -> task<void>
				{
					bool throws = false;
					MACORO_TRY{

						if (party)
							throw std::runtime_error(COPROTO_LOCATION);
						else
						{
							char c = co_await s.recv<char>();
							(void)c;
						}
					}
						MACORO_CATCH(exPtr)
					{
						throws = true;
						co_await s.close();
					}

					if (!throws)
						throw std::runtime_error(COPROTO_LOCATION);

					co_return;
				};


			for (auto t : types)
			{
				auto r = eval(proto, t);
				std::get<0>(r).result();
				std::get<1>(r).result();
			}
		}


		task<int> task_echoServer(Socket ss, u64 idx, u64 length, u64 rep, std::string name, bool v)
		{
#ifdef COPROTO_LOGGING
			auto np = name + "_server_" + std::to_string(idx) + "_" + std::to_string(length);
			co_await Name(np);
#endif

			auto exp = std::vector<char>(length);
			std::iota(exp.begin(), exp.end(), 0);

			if (v) {
				std::lock_guard<std::mutex> lock(gPrntMtx);
				std::cout << name << " s start " << idx << " " << length << std::endl;
			}

			if (v) {
				std::lock_guard<std::mutex> lock(gPrntMtx);
				std::cout << name << " s recv " << idx << " " << length << " begin" << std::endl;
			}
			auto msg = std::vector<char>();

			for (u64 i = 0; i < rep; ++i)
			{
				auto r = ss.recv<std::vector<char>>();
#ifdef COPROTO_LOGGING
				r.setName(np + "_r" + std::to_string(i));
#endif
				msg = co_await std::move(r);
				//msg = co_await recv<std::vector<char>>();
				if (exp != msg)
				{
					std::lock_guard<std::mutex> lock(gPrntMtx);
					std::cout << "bad msg " << COPROTO_LOCATION << std::endl;
					throw std::runtime_error("");
				}
			}

			if (v) {
				std::lock_guard<std::mutex> lock(gPrntMtx);
				std::cout << name << " s recv " << idx << " " << length << " done" << std::endl;
			}

			if (v) {
				std::lock_guard<std::mutex> lock(gPrntMtx);
				std::cout << name << " s send " << idx << " " << length << std::endl;
			}
			for (u64 i = 0; i < rep; ++i)
			{
				auto s = ss.send(msg);
#ifdef COPROTO_LOGGING
				s.setName(np + "_s" + std::to_string(i));
#endif
				co_await std::move(s);
			}

			if (v) {
				std::lock_guard<std::mutex> lock(gPrntMtx);
				std::cout << name << " s send " << idx << " " << length << " done" << std::endl;
			}

			//co_await EndOfRound();

			if (v) {
				std::lock_guard<std::mutex> lock(gPrntMtx);
				std::cout << name << " s EOR " << idx << " " << length << " " << std::endl;
			}

			if (idx)
			{
				co_return co_await task_echoServer(ss, idx - 1, length, rep, name, v);
			}
			else
			{
				if (v)
					std::cout << name << " ###################### s done " << idx << " " << length << std::endl;
				co_return 0;
			}
		}
		task<int> task_echoClient(Socket ss, u64 idx, u64 length, u64 rep, std::string name, bool v)
		{
			//			try {

#ifdef COPROTO_LOGGING
			auto np = name + "_client_" + std::to_string(idx) + "_" + std::to_string(length);
			co_await Name(np);
#endif
			if (v) {
				std::lock_guard<std::mutex> lock(gPrntMtx);
				std::cout << name << " c start " << idx << " " << length << std::endl;
			}
			auto msg = std::vector<char>(length);
			std::iota(msg.begin(), msg.end(), 0);
			if (v) {
				std::lock_guard<std::mutex> lock(gPrntMtx);
				std::cout << name << " c send " << idx << " " << length << std::endl;
			}
			for (u64 i = 0; i < rep; ++i)
			{
				auto s = ss.send(msg);
#ifdef COPROTO_LOGGING
				s.setName(np + "_s" + std::to_string(i));
#endif
				co_await std::move(s);
			}

			if (v) {
				std::lock_guard<std::mutex> lock(gPrntMtx);
				std::cout << name << " c send " << idx << " " << length << " done" << std::endl;
			}
			//co_await EndOfRound();

			if (v) {
				std::lock_guard<std::mutex> lock(gPrntMtx);
				std::cout << name << " c EOR " << idx << " " << length << " " << std::endl;
				std::cout << name << " c recv " << idx << " " << length << " begin" << std::endl;
			}
			for (u64 i = 0; i < rep; ++i)
			{

				auto r = ss.recv<std::vector<char>>();
#ifdef COPROTO_LOGGING
				r.setName(np + "_r" + std::to_string(i));
#endif
				std::vector<char> msg2;
				msg2 = co_await std::move(r);
				//auto msg2 = co_await recv<std::vector<char>>();
				if (msg2 != msg)
				{
					std::lock_guard<std::mutex> lock(gPrntMtx);
					std::cout << "bad msg " << COPROTO_LOCATION << std::endl;
					throw std::runtime_error(COPROTO_LOCATION);
				}
			}
			if (v) {
				std::lock_guard<std::mutex> lock(gPrntMtx);
				std::cout << name << " c recv " << idx << " " << length << " done" << std::endl;
			}
			if (idx)
			{
				co_return co_await task_echoClient(ss, idx - 1, length, rep, name, v);
			}
			else
			{

				if (v) {
					std::lock_guard<std::mutex> lock(gPrntMtx);
					std::cout << name << " ###################### c done " << idx << " " << length << std::endl;
				}
				co_return 0;
			}
			//			}
			//			catch (std::exception& e)
			//			{
			//				//std::cout << "exp "<< e.what() << std::endl;
			//				throw;
			//			}
		}


		void task_nestedProtocol_Test()
		{
			auto proto = [](Socket& s, bool party) -> task<void> {
				bool verbose = false;
				std::string str("hello from 0");
				u64 n = 5;
				if (party)
				{
					//std::cout << "p1 send " << std::endl;
					auto r = co_await macoro::wrap(s.send(std::move(str)));
					r.value();
					//throw std::runtime_error(COPROTO_LOCATION);

					co_await task_echoServer(s, n, 10, 1, "p1", verbose);
				}
				else
				{
					//std::cout << "p0 recv " << std::endl;
					co_await s.recv(str);
					//std::cout << " p0 recv" << std::endl;

					if (str != "hello from 0")
						throw std::runtime_error(COPROTO_LOCATION);

					co_await task_echoClient(s, n, 10, 1, "p0", verbose);
					//std::cout << " p0 sent" << std::endl;

				}
				};

			for (auto t : types)
			{
				auto r = eval(proto, t);
				std::get<0>(r).result();
				std::get<1>(r).result();
			}
		}
		struct ThrowServerException : public std::runtime_error
		{
			ThrowServerException(std::string s)
				: std::runtime_error(s)
			{}
		};

		task<void> task_throwServer(Socket& s, u64 i)
		{
			std::string msg;
			msg = co_await s.recv<std::string>();
			co_await s.send((msg));

			if (i)
				co_await task_throwServer(s, i - 1);
			else
				throw ThrowServerException(COPROTO_LOCATION);
		}

		task<void> task_throwClient(Socket& s, u64 i)
		{
			auto msg = std::string("hello world");
			co_await s.send(msg);
			if (msg != co_await s.recv<std::string>())
			{
				throw std::runtime_error("hello world");
			}

			if (i)
				co_await task_throwClient(s, i - 1);
		}

		void task_nestedProtocol_Throw_Test()
		{

			auto proto = [](Socket& s, bool party) -> task<void> {

				if (party)
				{
					std::vector<u64> buff(10);
					co_await s.send(buff);
					co_await task_throwServer(s, 4);
				}
				else
				{
					std::vector<u64> buff(10);
					co_await s.recv(buff);
					co_await task_throwClient(s, 4);
				}
				};

			for (auto t : types)
			{
				auto r = eval(proto, t);
				try
				{
					std::get<0>(r).result();
				}
				catch (std::system_error& e)
				{
					if (e.code() != code::remoteClosed)
						throw std::runtime_error(COPROTO_LOCATION);
				}
				try
				{
					std::get<1>(r).result();
				}
				catch (ThrowServerException&)
				{
				}
			}
		}

		void task_nestedProtocol_ErrorCode_Test()
		{

			u64 n = 2;
			auto proto = [n](Socket& s, bool party) -> task<void> {

				if (party)
				{
					std::vector<u64> buff(10);

					try {
						co_await s.send(buff);
					}
					catch (...)
					{
						std::cout << "ex on send" << std::endl;
						throw;
					}
					auto ec = co_await macoro::wrap(task_throwServer(s, n));

					if (ec.has_error())
					{

						auto error = ec.error();
						auto code = as_error_code(error);

						if (code != code::uncaughtException)
						{
							std::cout << " bad code " << code << " " << COPROTO_LOCATION << std::endl;
							throw std::runtime_error(COPROTO_LOCATION);
						}
					}
					else
					{
						std::cout << "no error" << COPROTO_LOCATION << std::endl;
						throw std::runtime_error(COPROTO_LOCATION);
					}
					//ec = co_await send(buff).wrap();

					//if(ec != code::ioError)

				}
				else
				{
					std::vector<u64> buff(10);
					try {
						co_await s.recv(buff);
					}
					catch (...)
					{
						std::cout << "ex on recv" << std::endl;
						throw;
					}

					co_await(task_throwClient(s, n) | macoro::wrap());
					//if (r.has_error() == false)
						//throw COPROTO_RTE;
				}
				};


			for (auto t : types)
			{
				auto r = eval(proto, t);
				std::get<0>(r).result();
				std::get<1>(r).result();
			}
		}




		void task_asyncProtocol_Test()
		{
			//#define MULTI
			//STExecutor ex;
			bool print = false;
			// level of recursion
			u64 n = 10;
			// number of messages sent at each level.
			u64 rep = 5;

			auto proto = [n, print, rep](Socket& s, bool party, macoro::thread_pool& ex) -> macoro::task<> {

				auto name = std::string{};
				auto buff = std::vector<u64>(10);
				//auto i = u64{};

				//co_await macoro::transfer_to(ex);



				if (party)
				{
					name = std::string("                  p1");

					co_await s.recv(buff);
					if (print)
					{
						std::lock_guard<std::mutex> lock(gPrntMtx);
						std::cout << "\n" << name << " mid" << std::endl;
					}
					//co_await (task_echoServer(s.fork(), n, 5, rep, name, print) 
					//	| macoro::start_on(ex) 
					//	| macoro::scoped());
					//co_await (task_echoServer(s.fork(), n + 2, 6, rep, name, print) 
					//	| macoro::start_on(ex)
					//	| macoro::scoped());
					//co_await (task_echoServer(s.fork(), n, 7, rep, name, print)
					//	| macoro::start_on(ex)
					//	| macoro::scoped());
					//co_await (task_echoServer(s.fork(), n + 7, 8, rep, name, print)
					//	| macoro::start_on(ex)
					//	| macoro::scoped());
					//co_await (task_echoServer(s.fork(), n, 9, rep, name, print)
					//	| macoro::start_on(ex)
					//	| macoro::scoped());

					co_await task_echoClient(s, n, 10, rep, name, print);

				}
				else
				{
					name = std::string("p0");

					co_await s.send(buff);

					if (print)
					{
						std::lock_guard<std::mutex> lock(gPrntMtx);
						std::cout << "\n" << name << " mid" << std::endl;
					}
					//co_await (task_echoClient(s.fork(), n, 5, rep, name, print)
					//	| macoro::start_on(ex)
					//	| macoro::scoped());
					//co_await (task_echoClient(s.fork(), n + 2, 6, rep, name, print)
					//	| macoro::start_on(ex)
					//	| macoro::scoped());
					//co_await (task_echoClient(s.fork(), n, 7, rep, name, print)
					//	| macoro::start_on(ex)
					//	| macoro::scoped());
					//co_await (task_echoClient(s.fork(), n + 7, 8, rep, name, print)
					//	| macoro::start_on(ex)
					//	| macoro::scoped());
					//co_await (task_echoClient(s.fork(), n, 9, rep, name, print)
					//	| macoro::start_on(ex)
					//	| macoro::scoped());
					co_await task_echoServer(s, n, 10, rep, name, print);
				}

				};

			for (auto t : { EvalTypes::async })
			{
				auto r = evalEx(proto, t);
				std::get<0>(r).result();
				std::get<1>(r).result();
			}
		}

		void task_asyncProtocol_Throw_Test()
		{

			u64 n = 3;
			auto proto = [n](Socket& s, bool party, macoro::thread_pool& ex) -> task<void> {
				co_await macoro::transfer_to(ex);

				if (party)
				{
					std::vector<u64> buff(10);
					co_await s.send(buff);

					co_await(task_throwServer(s, n) | macoro::start_on(ex));
				}
				else
				{
					std::vector<u64> buff(10);
					co_await s.recv(buff);
					co_await(task_throwClient(s, n) | macoro::start_on(ex));
				}

				};


			//for (auto t : types)
			for (auto t : { EvalTypes::async })
			{
				auto r = evalEx(proto, t);
				std::get<0>(r).result();

				try { std::get<1>(r).result(); }
				catch (ThrowServerException&) {}
			}
		}

		void task_errorSocket_Test()
		{

			//#define MULTI
			bool print = false;
			u64 n = 1;
			u64 rep = 1;
			auto proto = [n, print, rep](Socket& s, bool party, macoro::thread_pool& ex) -> task<void> {
				MACORO_TRY{
					co_await macoro::transfer_to(ex);

					if (party)
					{
						auto name = std::string("p1");
						//co_await Name(name);
						std::vector<u64> buff(10);



						co_await s.recv(buff);
						//co_await EndOfRound();

						auto fu0 = task_echoServer(s.fork(), n, 5, rep, name, print) | macoro::start_on(ex) | macoro::wrap();
						auto fu1 = task_echoServer(s.fork(), n + 2, 6, rep, name, print) | macoro::wrap() | macoro::start_on(ex);
						auto fu2 = task_echoServer(s.fork(), n, 7, rep, name, print) | macoro::wrap() | macoro::start_on(ex);

						auto r = co_await(task_echoClient(s, n, 10, rep, name, print) | macoro::wrap());
						//co_await send(buff);
						auto r0 = co_await std::move(fu0);
						auto r1 = co_await std::move(fu1);
						auto r2 = co_await std::move(fu2);

						r.value();
						r0.value();
						r1.value();
						r2.value();
					}
					else
					{
						auto name = std::string("p0");
						//co_await Name(name);
						std::vector<u64> buff(10);
						co_await s.send(buff);
						//co_await recv(buff);

						auto fu0 = task_echoClient(s.fork(), n, 5, rep, name, print) | macoro::wrap() | macoro::start_on(ex);
						auto fu1 = task_echoClient(s.fork(), n + 2, 6, rep, name, print) | macoro::wrap() | macoro::start_on(ex);
						auto fu2 = task_echoClient(s.fork(), n, 7, rep, name, print) | macoro::wrap() | macoro::start_on(ex);
						auto r = co_await(task_echoServer(s, n, 10, rep, name, print) | macoro::wrap());
						//co_await recv(buff);

						auto r0 = co_await std::move(fu0);
						auto r1 = co_await std::move(fu1);
						auto r2 = co_await std::move(fu2);

						r.value();
						r0.value();
						r1.value();
						r2.value();
					}
				}
					MACORO_CATCH(exPtr)
				{
					co_await s.close();
					std::rethrow_exception(exPtr);
				}
				};
#ifdef MULTI 
#undef MULTI
#endif

			for (auto type : types)
			{
				//std::array<Socket, 2> s;
				//STExecutor ex;
				u64 numOps = 0;
				if (type == EvalTypes::async)
				{
					//auto ss = LocalAsyncSocket::makePair(ex);
					//s[0] = ss[0];
					//s[1] = ss[1];

					macoro::thread_pool stx0;
					auto w0 = stx0.make_work();
					stx0.create_thread();

					if (1)
					{

						auto s = LocalAsyncSocket::makePair();
						s[0].mSock->errFn() = [&numOps]()->error_code {
							++numOps;
							return code::success;
							};
						s[1].mSock->errFn() = [&numOps]() ->error_code {
							++numOps;
							return code::success;
							};
						auto w = macoro::when_all_ready(proto(s[0], 0, stx0), proto(s[1], 1, stx0));
						auto r = macoro::sync_wait(std::move(w));
					}
					else
					{
						numOps = 140;
					}


					for (u64 i = 0; i < numOps; ++i)
					{
						//std::cout << i << std::endl;
						u64 idx = 0;
						auto s = LocalAsyncSocket::makePair();

						s[0].enableLogging();
						s[1].enableLogging();

						s[0].mSock->errFn() = [&idx, i]() ->error_code {
							if (idx++ == i)
								return code::DEBUG_ERROR;
							return code::success;
							};
						s[1].mSock->errFn() = [&idx, i]()->error_code {
							if (idx++ == i)
							{
								return code::DEBUG_ERROR;
							}
							return code::success;
							};
						auto w = macoro::when_all_ready(proto(s[0], 0, stx0), proto(s[1], 1, stx0));
						auto r = macoro::sync_wait(std::move(w));

						error_code e0(code::success), e1(code::success);
						try { std::get<0>(r).result(); }
						catch (std::system_error& e) { e0 = e.code(); }
						try { std::get<1>(r).result(); }
						catch (std::system_error& e) { e1 = e.code(); }

						if (!e0 && !e1)
							throw std::runtime_error("error was expected: \n" +
								e0.message() + "\n" + e1.message());
					}

					w0 = {};
					stx0.join();
				}
				else
				{

					//auto ss = LocalBlockingSock::makePair();
					//s[0] = ss[0];
					//s[1] = ss[1];
				}


				//LocalEvaluator eval;
				//auto s = eval.getSocketPair(type);
				//auto p0 = proto(s[0], 0);
				//auto p1 = proto(s[1], 1);
				//auto ec = eval.eval(p0, p1, type);
				//auto numOps = eval.mOpIdx;

				//if (ec.mRes0 || ec.mRes1)
				//	throw std::runtime_error(ec.message());

				//for (u64 i = 0; i < numOps; ++i)
				//{
				//	LocalEvaluator eval;
				//	auto s = eval.getSocketPair(type);
				//	auto p0 = proto(s[0], 0);
				//	auto p1 = proto(s[1], 1);

				//	eval.mErrorIdx = i;
				//	auto ec = eval.eval(p0, p1, type);


				//	if (ec.mRes0 != code::DEBUG_ERROR && ec.mRes1 != code::DEBUG_ERROR)
				//		throw std::runtime_error("error was expected: " + ec.message());
				//}


			}

				}

		void task_cancel_send_test()
		{

			macoro::stop_source src;
			auto token = src.get_token();

			auto proto = [&](Socket& s, bool party) -> task<void>
				{
					int i;
					co_await s.send(i, token);
				};

			{

				auto s = LocalAsyncSocket::makePair();
				auto b = macoro::make_blocking(proto(s[0], 0));
				src.request_stop();
				try { b.get(); }
				catch (std::system_error& e) {
					auto ec = e.code();
					TEST(ec == code::operation_aborted);
				}
			}
		}

		void task_cancel_recv_test()
		{

			macoro::stop_source src;
			auto token = src.get_token();

			auto proto = [&](Socket& s, bool party) -> task<void>
				{
					int i;
					co_await s.recv(i, token);
				};

			{
				auto s = LocalAsyncSocket::makePair();
				auto b = macoro::make_blocking(proto(s[0], 0));
				src.request_stop();
				try { b.get(); }
				catch (std::system_error& e) { TEST(e.code() == code::operation_aborted); }
			}
		}

		void task_destroySelf_test()
		{

			auto proto = [&](Socket s, bool party) -> task<void>
				{
					int i;
					if (party)
						co_await s.recv(i);
					else
						co_await s.send(i);

				};

			{
				auto socks = LocalAsyncSocket::makePair();
				auto b = macoro::sync_wait(macoro::when_all_ready(proto(std::move(socks[0]), 0), proto(std::move(socks[1]), 1)));
				std::get<0>(b).result();
				std::get<1>(b).result();
			}
		}

#endif
		}

	}