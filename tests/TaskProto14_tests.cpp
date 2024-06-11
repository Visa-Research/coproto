
#include "TaskProto_tests.h"
#include "coproto/Socket/Socket.h"
#include "coproto/Socket/LocalAsyncSock.h"
#include "macoro/task.h"
#include "macoro/stop.h"
#include "macoro/sync_wait.h"
#include "macoro/when_all.h"
#include "macoro/wrap.h"
#include "macoro/start_on.h"
#include "macoro/timeout.h"
#include "macoro/take_until.h"
#include "macoro/thread_pool.h"
#include <numeric>
#include "eval.h"
#include "TaskProto14_tests.h"

#define TEST(X) if(!(X)) throw COPROTO_RTE

namespace coproto
{

	namespace tests
	{
		namespace {
			std::mutex gPrntMtx;
			auto types = {
				EvalTypes::async,
				EvalTypes::Buffering
				//EvalTypes::blocking
			};
		}

		void task14_proto_test()
		{
			for (auto type : types)//{EvalTypes::blocking})
			{

				//int recv = 0;
				//std::array<bool, 2> started = { false,false };

				auto task_proto = [&](Socket ss, bool send)->task<void>
					{
						MC_BEGIN(task<void>, ss, send, cc = int{}/*, &started, &recv*/);
						if (send)
						{
							//started[0] = true;
							cc = 42;
							MC_AWAIT(ss.send(cc));
						}
						else
						{
							//started[1] = true;
							MC_AWAIT(ss.recv(cc));
							//recv = cc;
						}

						MC_END();
					};

				auto r = eval(task_proto, type);
				std::get<0>(r).result();
				std::get<1>(r).result();
			}
		}


		void task14_strSendRecv_Test()
		{
			bool verbose = false;
			//int sDone = 0, rDone = 0;
			auto proto = [&](Socket& s, bool party) -> task<void> {

				MC_BEGIN(task<>, &, party
					, sStr = std::string("hello from 0")
					, sRtr = std::string("hello from _")
					, i = u64{}
				);

				for (i = 0; i < 5; ++i)
				{
					if (party)
					{
						MC_AWAIT(s.send(sStr));

						if (verbose)
							std::cout << "s send " << std::endl;
						//co_await EndOfRound();
						if (verbose)
							std::cout << "s eor " << std::endl;

						MC_AWAIT_SET(sStr, s.recv<std::string>());
						if (verbose)
							std::cout << "s recv " << std::endl;

						if (sStr != "hello from " + std::to_string(i * 2 + 1))
							throw std::runtime_error(COPROTO_LOCATION);
						sStr.back() += 1;
					}
					else
					{
						MC_AWAIT(s.recv(sRtr));
						if (verbose)
							std::cout << "r recv " << std::endl;

						if (sRtr != "hello from " + std::to_string(i * 2 + 0))
							throw std::runtime_error(COPROTO_LOCATION);

						sRtr.back() += 1;
						MC_AWAIT(s.send(sRtr));
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

				MC_END();

				};



			for (auto t : types)
			{
				auto r = eval(proto, t);
				std::get<0>(r).result();
				std::get<1>(r).result();
			}

		}
		void task14_resultSendRecv_Test()
		{

			auto proto = [](Socket& sock, bool party) -> task<void> {

				MC_BEGIN(task<>, sock, party
					, str = std::string("hello from 0")
					, ec = macoro::result<void>{}
					, r = macoro::result<std::string>{}
					, i = u64{}
				);
				//co_await Name("main");

				for (i = 0; i < 5; ++i)
				{
					if (party)
					{
						//std::cout << " p1 sent " << i << std::endl;
						MC_AWAIT_SET(ec, macoro::wrap(sock.send(str)));
						//std::cout << " p1 sent " << i << " ok" << std::endl;

						//std::cout << " p1 recv " << i << std::endl;
						MC_AWAIT_SET(r, sock.recv<std::string>() | macoro::wrap());
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
						MC_AWAIT(sock.recv(str));
						//std::cout << " p0 recv " << i << " ok" << std::endl;

						if (str != "hello from " + std::to_string(i * 2 + 0))
							throw std::runtime_error(COPROTO_LOCATION);

						str.back() += 1;

						//std::cout << " p0 sent " << i << std::endl;
						MC_AWAIT(sock.send(str));
						//std::cout << " p0 sent " << i << " ok" << std::endl;

					}
				}

				MC_END();
				};


			for (auto t : types)
			{
				auto r = eval(proto, t);
				std::get<0>(r).result();
				std::get<1>(r).result();
			}
		}

		void task14_typedRecv_Test()
		{
			bool verbose = false;
			auto proto = [verbose](Socket& sock, bool party) -> task<void> {
				MC_BEGIN(task<>, verbose, sock, party
					, buff = std::vector<u64>{}
					, rBuff = std::vector<u64>{}
					, i = u64{}
				);
				for (i = 0; i < 5; ++i)
				{
					if (party)
					{
						buff.resize(1 + i * 2);
						std::fill(buff.begin(), buff.end(), i * 2);
						if (verbose)std::cout << "send0 b " << i << std::endl;
						MC_AWAIT(sock.send(std::move(buff)));
						if (verbose)std::cout << "send0 e " << i << std::endl;
						if (verbose)std::cout << "recv1 b " << i << std::endl;

						MC_AWAIT_SET(rBuff, sock.recv<std::vector<u64>>());

						if (verbose)std::cout << "recv1 e " << i << std::endl;

						buff.resize(2 + i * 2);
						std::fill(buff.begin(), buff.end(), i * 2 + 1);

						if (buff != rBuff)
							throw std::runtime_error(COPROTO_LOCATION);
					}
					else
					{
						if (verbose)std::cout << "recv0 b " << i << std::endl;
						MC_AWAIT_SET(rBuff, sock.recv<std::vector<u64>>());
						if (verbose)std::cout << "recv0 e " << i << std::endl;
						buff.resize(1 + i * 2);
						std::fill(buff.begin(), buff.end(), i * 2);

						if (buff != rBuff)
							throw std::runtime_error(COPROTO_LOCATION);

						buff.resize(buff.size() + 1);
						std::fill(buff.begin(), buff.end(), i * 2 + 1);
						if (verbose)std::cout << "send1 b " << i << std::endl;
						MC_AWAIT(sock.send(std::move(buff)));
						if (verbose)std::cout << "send1 e " << i << std::endl;

					}
				}

				MC_END();
				};


			for (auto t : types)
			{
				eval(proto, t);
			}
		}


		void task14_zeroSendRecv_Test()
		{

			auto proto = [](Socket& s, bool party) -> task<void> {

				MC_BEGIN(task<>, s, party
					, buff = std::vector<u64>{}
				);
				MC_AWAIT(s.send(buff));
				MC_END();
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

		void task14_zeroSendRecv_ErrorCode_Test()
		{

			auto proto = [](Socket& s, bool party) -> task<void> {

				MC_BEGIN(task<>, s, party
					, buff = std::vector<u64>{}
					, ec = macoro::result<void>{}
				);

				MC_AWAIT_SET(ec, s.send(buff) | macoro::wrap());

				if (as_error_code(ec.error()) != code::sendLengthZeroMsg)
					throw std::runtime_error("");

				MC_END();
				};
			for (auto t : types)
			{
				auto r = eval(proto, t);
				std::get<0>(r).result();
				std::get<1>(r).result();
			}
		}

		void task14_badRecvSize_Test()
		{
			auto proto = [](Socket& s, bool party) -> task<void> {
				MC_BEGIN(task<>, =
					, buff = std::vector<u64>(3)
				);

				if (party)
				{
					MC_AWAIT(s.send(buff));
				}
				else
				{
					buff.resize(1);
					MC_AWAIT(s.recv(buff));
				}
				MC_END();
				};

			for (auto t : types)
			{
				auto r = eval(proto, t);


				try { std::get<0>(r).result(); throw std::runtime_error(""); }
				catch (BadReceiveBufferSize& b) {
					if (b.mBufferSize != 8 || b.mReceivedSize != 24)
						throw;
				};
				try {
					std::get<1>(r).result();

					if (t != EvalTypes::Buffering)
						throw std::runtime_error("");
				}
				catch (std::system_error& e)
				{
					if (e.code() != code::remoteClosed)
						throw;
				}
			}
		}

		void task14_badRecvSize_ErrorCode_Test()
		{
			for (auto t : types)
			{
				auto proto = [t](Socket& s, bool party) -> task<void> {
					MC_BEGIN(task<>, =
						, buff = std::vector<u64>(3)
						, r = macoro::result<void>{}
					);

					if (party)
					{
						MC_AWAIT_SET(r, s.send(buff) | macoro::wrap());

						if (t != EvalTypes::Buffering &&
							as_error_code(r.error()) != code::remoteClosed)
							throw std::runtime_error("");

						MC_AWAIT_SET(r, s.send(buff) | macoro::wrap());
						if (t != EvalTypes::Buffering &&
							as_error_code(r.error()) != code::cancel)
							throw std::runtime_error("");

					}
					else
					{
						buff.resize(1);
						MC_AWAIT_SET(r, s.recv(buff) | macoro::wrap());

						if (as_error_code(r.error()) != code::badBufferSize)
							throw std::runtime_error("");


						MC_AWAIT_SET(r, macoro::wrap(s.recv(buff)));
						if (as_error_code(r.error()) != code::cancel)
							throw std::runtime_error("");

						MC_AWAIT_SET(r, macoro::wrap(s.send(buff)));
						if (as_error_code(r.error()) != code::cancel)
							throw std::runtime_error("");
					}

					MC_END();
					};


				auto r = eval(proto, t);
				std::get<0>(r).result();
				std::get<1>(r).result();
			}
		}

		void task14_moveSend_Test()
		{
			for (auto t : types)
			{
				auto proto = [t](Socket& s, bool party) -> task<void>
					{
						MC_BEGIN(task<>, =
							, i = int{ 0 }
							, v = std::vector<int>(10)
							, t0 = macoro::eager_task<void>{}
							, t1 = macoro::eager_task<void>{}
						);

						//if (t == EvalTypes::Buffering)
						//{
						//	t0 = (s.send(std::move(i)) | macoro::start_on(ex));
						//	MC_AWAIT(s.recv(i));
						//	macoro::sync_wait(t0);

						//	t1 = (s.send(std::move(v)) | macoro::start_on(ex));
						//	MC_AWAIT(s.recvResize(v));
						//	macoro::sync_wait(t1);
						//}
						//else
						{
							MC_AWAIT(s.send(std::move(i)));
							MC_AWAIT(s.recv(i));
							MC_AWAIT(s.send(std::move(v)));
							MC_AWAIT(s.recvResize(v));
						}

						MC_END();
					};

				//auto work = ex.make_work();
				//if (t == EvalTypes::Buffering)
				//	ex.create_thread();

				auto r = eval(proto, t);

				std::get<0>(r).result();
				std::get<1>(r).result();
			}
		}

		void task14_throws_Test()
		{

			auto proto = [](Socket& s, bool party) -> task<void>
				{
					MC_BEGIN(task<>, =
						, c = macoro::result<char>{}
					);

					if (party)
					{
						MC_AWAIT(s.close());
					}
					else
					{
						//throw std::runtime_error("");
						MC_AWAIT_SET(c, s.recv<char>() | macoro::wrap());

						if (c.has_error())
							MC_AWAIT(s.close());
					}

					MC_END();
				};


			for (auto t : types)
			{
				auto r = eval(proto, t);
				std::get<0>(r).result();
				std::get<1>(r).result();
			}
		}


		task<int> task14_echoServer(
			Socket ss,
			u64 idx,
			u64 length,
			u64 rep,
			std::string name,
			bool v
		/*, std::vector<std::string>* log = nullptr*/)
		{
			MC_BEGIN(task<int>, =
				, exp = std::vector<char>(length)
				, msg = std::vector<char>()
				, i = u64{}
			);

			std::iota(exp.begin(), exp.end(), 0);

			if (v) {
				std::lock_guard<std::mutex> lock(gPrntMtx);
				std::cout << name << " s start " << idx << " " << length << std::endl;
			}

			if (v) {
				std::lock_guard<std::mutex> lock(gPrntMtx);
				std::cout << name << " s recv " << idx << " " << length << " begin" << std::endl;
			}

			for (i = 0; i < rep; ++i)
			{
				//if(log)
				//	log->push_back("srv-")
				MC_AWAIT_SET(msg, ss.recv<std::vector<char>>());
				if (exp != msg)
				{
					std::lock_guard<std::mutex> lock(gPrntMtx);
					std::cout << "bad msg " << COPROTO_LOCATION << std::endl;
					std::cout << "exp " << exp.size() << " " << std::string(exp.begin(), exp.end()) << std::endl;
					std::cout << "msg " << msg.size() << " " << std::string(msg.begin(), msg.end()) << std::endl;

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
			for (i = 0; i < rep; ++i)
			{
				MC_AWAIT(ss.send(msg));
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
				MC_RETURN_AWAIT(task14_echoServer(ss, idx - 1, length, rep, name, v));
			}
			else
			{
				if (v)
					std::cout << name << " ###################### s done " << idx << " " << length << std::endl;
				MC_RETURN(0);
			}

			MC_END();
		}

		task<int> task14_echoClient(Socket ss, u64 idx, u64 length, u64 rep, std::string name, bool v)
		{
			MC_BEGIN(task<int>, =
				, msg = std::vector<char>(length)
				, msg2 = std::vector<char>{}
				, i = u64{}
			);
			if (v) {
				std::lock_guard<std::mutex> lock(gPrntMtx);
				std::cout << name << " c start " << idx << " " << length << std::endl;
			}
			std::iota(msg.begin(), msg.end(), 0);
			if (v) {
				std::lock_guard<std::mutex> lock(gPrntMtx);
				std::cout << name << " c send " << idx << " " << length << std::endl;
			}
			for (i = 0; i < rep; ++i)
			{
				MC_AWAIT(ss.send(msg));
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
			for (i = 0; i < rep; ++i)
			{

				MC_AWAIT_SET(msg2, ss.recv<std::vector<char>>());

				if (msg2 != msg)
				{
					std::lock_guard<std::mutex> lock(gPrntMtx);
					std::cout << "bad msg " << COPROTO_LOCATION << std::endl;
					throw std::runtime_error("");
				}
			}
			if (v) {
				std::lock_guard<std::mutex> lock(gPrntMtx);
				std::cout << name << " c recv " << idx << " " << length << " done" << std::endl;
			}
			if (idx)
			{
				MC_RETURN_AWAIT(task14_echoClient(ss, idx - 1, length, rep, name, v));
			}
			else
			{

				if (v) {
					std::lock_guard<std::mutex> lock(gPrntMtx);
					std::cout << name << " ###################### c done " << idx << " " << length << std::endl;
				}
				MC_RETURN(0);
			}

			MC_END();
		}


		void task14_nestedProtocol_Test()
		{
			bool verbose = false;
			auto proto = [verbose](Socket& s, bool party) -> task<void> {
				MC_BEGIN(task<>, =
					, str = std::string("hello from 0")
					, n = u64(5)
					, r = macoro::result<void>{}
				);
				if (party)
				{
					//std::cout << "p1 send " << std::endl;
					MC_AWAIT_SET(r, macoro::wrap(s.send(std::move(str))));
					r.value();
					//throw std::runtime_error(COPROTO_LOCATION);

					MC_AWAIT(task14_echoServer(s, n, 10, 1, "p1", verbose));
				}
				else
				{
					//std::cout << "p0 recv " << std::endl;
					MC_AWAIT(s.recv(str));
					//std::cout << " p0 recv" << std::endl;

					if (str != "hello from 0")
						throw std::runtime_error(COPROTO_LOCATION);

					MC_AWAIT(task14_echoClient(s, n, 10, 1, "p0", verbose));
					//std::cout << " p0 sent" << std::endl;

				}

				MC_END();
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

		task<void> task14_throwServer(Socket& s, u64 i)
		{
			MC_BEGIN(task<>, =
				, msg = std::string{}
			);
			MC_AWAIT_SET(msg, s.recv<std::string>());
			MC_AWAIT(s.send(msg));

			if (i)
			{
				MC_AWAIT(task14_throwServer(s, i - 1));
			}
			else
				throw ThrowServerException(COPROTO_LOCATION);
			MC_END();
		}

		task<void> task14_throwClient(Socket& s, u64 i)
		{
			MC_BEGIN(task<>, =
				, msg = std::string("hello world")
				, msg2 = std::string{}
			);
			MC_AWAIT(s.send(msg));
			MC_AWAIT_SET(msg2, s.recv<std::string>());

			if (msg != msg2)
			{
				throw std::runtime_error("hello world");
			}

			if (i)
				MC_AWAIT(task14_throwClient(s, i - 1));

			MC_END();
		}

		void task14_nestedProtocol_Throw_Test()
		{

			auto proto = [](Socket& s, bool party) -> task<void> {
				MC_BEGIN(task<>, =
					, buff = std::vector<u64>(10)
				);
				if (party)
				{
					MC_AWAIT(s.send(buff));
					MC_AWAIT(task14_throwServer(s, 4));
				}
				else
				{
					MC_AWAIT(s.recv(buff));
					MC_AWAIT(task14_throwClient(s, 4));
				}

				MC_END();
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

		void task14_nestedProtocol_ErrorCode_Test()
		{

			bool hasEc = false;
			u64 n = 5;
			auto proto = [&hasEc, n](Socket& s, bool party) -> task<void> {

				MC_BEGIN(task<>, =, &hasEc
					, buff = std::vector<u64>(10)
					, ec = macoro::result<void>{}
				);
				if (party)
				{
					MC_AWAIT(s.send(buff));
					MC_AWAIT_SET(ec, macoro::wrap(task14_throwServer(s, n)));

					if (as_error_code(ec.error()) == code::uncaughtException)
						hasEc = true;
				}
				else
				{
					MC_AWAIT(s.recv(buff));
					MC_AWAIT(task14_throwClient(s, n));
				}

				MC_END();
				};


			for (auto t : types)
			{
				auto r = eval(proto, t);
				std::get<0>(r).result();
				std::get<1>(r).result();
			}
		}




		void task14_asyncProtocol_Test()
		{

			//#define MULTI
						//STExecutor ex;
			bool print = false;
			// level of recursion
			u64 n = 10;
			// number of messages sent at each level.
			u64 rep = 5;

			auto proto = [n, print, rep](Socket& s, bool party, macoro::thread_pool& ex) -> task<void> {

				MC_BEGIN(task<>, =, &ex
					, name = std::string{}
					, buff = std::vector<u64>(10)
					, fus = std::vector<macoro::eager_task<int>>{}
					, i = u64{}
				);

				MC_AWAIT(macoro::transfer_to(ex));

				if (party)
				{
					name = std::string("                  p1");

					MC_AWAIT(s.recv(buff));
					if (print)
					{
						std::lock_guard<std::mutex> lock(gPrntMtx);
						std::cout << "\n" << name << " mid" << std::endl;
					}
					fus.emplace_back(task14_echoServer(s.fork(), n, 5, rep, name, print) | macoro::start_on(ex));
					//#ifdef MULTI
					fus.emplace_back(task14_echoServer(s.fork(), n + 2, 6, rep, name, print) | macoro::start_on(ex));
					fus.emplace_back(task14_echoServer(s.fork(), n, 7, rep, name, print) | macoro::start_on(ex));
					fus.emplace_back(task14_echoServer(s.fork(), n + 7, 8, rep, name, print) | macoro::start_on(ex));
					fus.emplace_back(task14_echoServer(s.fork(), n, 9, rep, name, print) | macoro::start_on(ex));
					//#endif

					MC_AWAIT(task14_echoClient(s, n, 10, rep, name, print));

					for (i = 0; i < fus.size(); ++i)
						MC_AWAIT(fus[i]);
				}
				else
				{
					name = std::string("p0");

					MC_AWAIT(s.send(buff));

					if (print)
					{
						std::lock_guard<std::mutex> lock(gPrntMtx);
						std::cout << "\n" << name << " mid" << std::endl;
					}
					fus.emplace_back(task14_echoClient(s.fork(), n, 5, rep, name, print) | macoro::start_on(ex));
					//#ifdef MULTI
					fus.emplace_back(task14_echoClient(s.fork(), n + 2, 6, rep, name, print) | macoro::start_on(ex));
					fus.emplace_back(task14_echoClient(s.fork(), n, 7, rep, name, print) | macoro::start_on(ex));
					fus.emplace_back(task14_echoClient(s.fork(), n + 7, 8, rep, name, print) | macoro::start_on(ex));
					fus.emplace_back(task14_echoClient(s.fork(), n, 9, rep, name, print) | macoro::start_on(ex));
					//#endif
					MC_AWAIT(task14_echoServer(s, n, 10, rep, name, print));
					//co_await recv(buff);

					for (i = 0; i < fus.size(); ++i)
						MC_AWAIT(fus[i]);
				}

				MC_END();
				};
			//#ifdef MULTI 
			//#undef MULTI
			//#endif

			for (auto t : { EvalTypes::async })
			{
				auto r = evalEx(proto, t);
				std::get<0>(r).result();
				std::get<1>(r).result();
			}
		}

		void task14_asyncProtocol_Throw_Test()
		{

			u64 n = 3;
			auto proto = [n](Socket& s, bool party, macoro::thread_pool& ex) -> task<void> {
				MC_BEGIN(task<>, =, &ex
					, buff = std::vector<u64>(10)
				);
				MC_AWAIT(macoro::transfer_to(ex));

				if (party)
				{
					MC_AWAIT(s.send(buff));
					MC_AWAIT(task14_throwServer(s, n) | macoro::start_on(ex));
				}
				else
				{
					MC_AWAIT(s.recv(buff));
					MC_AWAIT(task14_throwClient(s, n) | macoro::start_on(ex));
				}
				MC_END();
				};


			for (auto t : { EvalTypes::async })
			{
				auto r = evalEx(proto, t);
				std::get<0>(r).result();

				try { std::get<1>(r).result(); }
				catch (ThrowServerException&) {}
			}
		}

		void task14_endOfRound_Test()
		{
			return;
			u64 done = 0;

			auto proto = [&done](Socket& s, bool party, macoro::thread_pool& sched) -> task<void> {

				MC_BEGIN(task<>, =, &done, &sched
					, i = int{}
					, j = u64{}
				);
				if (party)
				{
					MC_AWAIT(s.send(42));
					MC_AWAIT(sched.post());
					for (j = 0; j < 10; ++j)
					{
						MC_AWAIT(s.recv(i));
						MC_AWAIT(s.send(42));
						MC_AWAIT(sched.post());
					}
				}
				else
				{
					for (j = 0; j < 10; ++j)
					{
						MC_AWAIT(s.recv(i));
						MC_AWAIT(s.send(42));
						MC_AWAIT(sched.post());
					}

					MC_AWAIT(s.recv(i));
				}
				++done;

				MC_END();
				};


			{
				//STExecutor ex;
				//auto s = RoundFunctionSock::makePair();
				//auto t0 = proto(s[0], 0, ex) | macoro::start_on(ex);
				//auto t1 = proto(s[1], 1, ex) | macoro::start_on(ex);

				//while (ex.mQueue_.size() ||
				//	s[0].mImpl->mInbound.size() ||
				//	s[1].mImpl->mInbound.size())
				//{
				//	ex.run();
				//	s[1].setInbound(s[0].getOutbound());
				//	s[0].setInbound(s[1].getOutbound());
				//}

				//auto breakFn = [&]() { return ex.mQueue_.size() == 0; };

				//if (done != 2)
				//	throw COPROTO_RTE;
			}
		}

		void task14_errorSocket_Test()
		{
			//#define MULTI
			bool print = false;
			u64 n = 1;
			u64 rep = 1;
			auto proto = [n, print, rep](Socket& s, bool party, macoro::thread_pool& ex) -> task<void> {
				MC_BEGIN(task<>, =, &ex
					, name = std::string{}
					, buff = std::vector<u64>(10)
					, fus = std::vector<macoro::eager_task<macoro::result<int>>>{}
					, rs = std::vector<macoro::result<int>>{}
					, r = macoro::result<void>{}
					, tt = task<macoro::result<int>>{}
					, i = u64{}
				);

				//MC_AWAIT(macoro::transfer_to(ex));

				if (party)
				{
					name = std::string("p1");
					//co_await Name(name);

					MC_AWAIT_SET(r, s.recv(buff) | macoro::wrap());
					if (r.has_error())
						MC_AWAIT(s.close());
					r.value();


					fus.emplace_back(
					task14_echoServer(s.fork(), n, 5, rep, name+".0", print)
						| macoro::wrap()
						| macoro::start_on(ex));


					fus.emplace_back(task14_echoServer(s.fork(), n + 2, 6, rep, name + ".1", print) | macoro::wrap() | macoro::start_on(ex));
					fus.emplace_back(task14_echoServer(s.fork(), n, 7, rep, name + ".2", print) | macoro::wrap() | macoro::start_on(ex));

					//MC_AWAIT_FN(rs.emplace_back, task14_echoClient(s, n, 10, rep, name + ".3", print) | macoro::wrap());

					for (i = 0; i < fus.size(); ++i)
						MC_AWAIT_FN(rs.emplace_back, fus[i]);

					for (i = 0; i < rs.size(); ++i)
					{
						if (rs[i].has_error() && !s.closed()) 
							MC_AWAIT(s.close());

						rs[i].value();
					}
				}
				else
				{
					name = std::string("p0");
					MC_AWAIT_SET(r, s.send(buff) | macoro::wrap());
					if (r.has_error())
						MC_AWAIT(s.close());
					r.value();

					fus.emplace_back(
						task14_echoClient(s.fork(), n, 5, rep, name + ".0", print)
						| macoro::wrap()
						| macoro::start_on(ex));
					fus.emplace_back(task14_echoClient(s.fork(), n + 2, 6, rep, name + ".1", print) | macoro::wrap() | macoro::start_on(ex));
					fus.emplace_back(task14_echoClient(s.fork(), n, 7, rep, name + ".2", print) | macoro::wrap() | macoro::start_on(ex));

					//MC_AWAIT_FN(rs.emplace_back, task14_echoServer(s, n, 10, rep, name + ".3", print) | macoro::wrap());

					for (i = 0; i < fus.size(); ++i)
						MC_AWAIT_FN(rs.emplace_back, fus[i]);

					for (i = 0; i < rs.size(); ++i)
					{
						if (rs[i].has_error() && !s.closed())
							MC_AWAIT(s.close());

						rs[i].value();
					}
				}

				MC_END();
			};
#ifdef MULTI 
#undef MULTI
#endif


			for (auto type : types)
			{
				u64 numOps = 0;
				if (type == EvalTypes::async)
				{

					//InlineExecutor stx0;
					macoro::thread_pool stx0;
					auto w0 = stx0.make_work();
					stx0.create_thread();

					auto s = LocalAsyncSocket::makePair();
					s[0].mSock->errFn() = [&numOps]()->error_code {
						++numOps;
						return code::success;
					};
					s[1].mSock->errFn() = [&numOps]() ->error_code {
						++numOps;
						return code::success;
					};
					s[0].enableLogging();
					s[1].enableLogging();
					auto w = macoro::when_all_ready(proto(s[0], 0, stx0), proto(s[1], 1, stx0));
					auto r = macoro::sync_wait(std::move(w));


					for (u64 i = 0; i < numOps; ++i)
					{
						u64 idx = 0;
						auto s = LocalAsyncSocket::makePair();
						bool hasError = false;
						s[0].mSock->errFn() = [&idx, i, &hasError]() ->error_code {
							if (idx++ == i)
							{
								hasError = true;
								return code::DEBUG_ERROR;
							}
							return code::success;
						};
						s[1].mSock->errFn() = [&idx, i, &hasError]()->error_code {
							if (idx++ == i)
							{
								hasError = true;
								return code::DEBUG_ERROR;
							}
							return code::success;
						};

						//std::cout << "-------"<<i<<"--------" << std::endl;
						auto w = macoro::when_all_ready(proto(s[0], 0, stx0), proto(s[1], 1, stx0));
						auto r = macoro::sync_wait(std::move(w));

						error_code e0(code::success), e1(code::success);
						try { std::get<0>(r).result(); }
						catch (std::system_error& e) { e0 = e.code(); }
						try { std::get<1>(r).result(); }
						catch (std::system_error& e) { e1 = e.code(); }

						if (!e0 && !e1)
						{
							std::cout << "i " << i << " " << hasError << std::endl;
							std::cout << "error was expected: \n" <<
								e0.message() << "\n" << e1.message() << std::endl;
							throw MACORO_RTE_LOC;
						}
					}

				}
				else
				{
				}
			}
		}

		void task14_cancel_send_test()
		{
			macoro::stop_source src;
			auto token = src.get_token();
			auto proto = [&](Socket& s, bool party) -> task<void>
				{
					MC_BEGIN(task<>, &s, &token, i = int{});

					MC_AWAIT(s.send(i, token));

					MC_END();
				};

			for (auto t : types)
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

		void task14_cancel_recv_test()
		{

			macoro::stop_source src;
			auto token = src.get_token();

			auto proto = [&](Socket& s, bool party) -> task<void>
				{
					MC_BEGIN(task<>, =, i = int{});
					MC_AWAIT(s.recv(i, token));
					MC_END();
				};

			for (auto t : types)
			{
				auto s = LocalAsyncSocket::makePair();
				auto b = macoro::make_blocking(proto(s[0], 0));
				src.request_stop();
				try { b.get(); }
				catch (std::system_error& e) { TEST(e.code() == code::operation_aborted); }

			}
		}

		void task14_timeout_test()
		{
			auto maxLag = 20;
			macoro::stop_source src;
			auto token = src.get_token();
			macoro::thread_pool ios;
			auto w = ios.make_work();
			ios.create_thread();
			static std::chrono::time_point<std::chrono::steady_clock> start, end;
			auto proto = [&](Socket& s, bool party) -> task<void>
				{
					MC_BEGIN(task<>, &s, &ios, i = int{});

					start = std::chrono::steady_clock::now();
					MC_AWAIT(s.recv(i, macoro::timeout(ios, std::chrono::milliseconds(15))));
					end = std::chrono::steady_clock::now();

					MC_END();
				};

			for (auto t : types)
			{

				auto r = eval(proto, t);

				auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
				//std::cout << "time " <<  << "ms" << std::endl;

				if (dur > 15 + maxLag)
					throw MACORO_RTE_LOC;

				try {
					std::get<0>(r).result(); 
					throw MACORO_RTE_LOC;
				}
				catch (std::system_error& e) {

					if (e.code() != code::operation_aborted)
					{
						std::cout << e.code().category().name() << std::endl;
						std::cout << e.code().message() << std::endl;
						throw;
					}
				}
				try { 
					std::get<1>(r).result(); 
					throw MACORO_RTE_LOC;
				}
				catch (std::system_error& e) {
					auto ec = e.code();
					if (t == EvalTypes::async)
					{
						if (ec != code::remoteClosed)
							throw;
					}
					else
					{
						if (ec != code::operation_aborted)
							throw;
					}
				}
			}

			w = {};
			ios.join();
		}

	}

}