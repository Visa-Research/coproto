
#include "BufferingSocket_tests.h"
#include "coproto/Socket/BufferingSocket.h"
#include "macoro/thread_pool.h"
#include "macoro/start_on.h"

namespace coproto
{
	namespace tests
	{

		void BufferingSocket_sendRecv_test()
		{
			std::array<BufferingSocket, 2> s;

			s[0].enableLogging();
			s[1].enableLogging();

			std::vector<u8> sb(10), rb(10);
			sb[4] = 5;
			auto a0 = s[0].mSock->send(sb);
			auto a1 = s[1].mSock->recv(rb);
			int done = 0;
			auto task_ = [&](bool sender) -> task<void> {

				MC_BEGIN(task<>, &, sender);
				if (sender)
					MC_AWAIT(a0);
				else
					MC_AWAIT(a1);
				++done;
				MC_END();
			};


			auto t = macoro::make_blocking(macoro::when_all_ready(
				task_(0),
				task_(1)
			));

			BufferingSocket::exchangeMessages(s[0], s[1]);

			auto r = t.get();
			std::get<0>(r).result();
			std::get<1>(r).result();

			if (done != 2)
				throw COPROTO_RTE;
			if (sb != rb)
				throw MACORO_RTE_LOC;
		}


		void BufferingSocket_asyncSend_test()
		{
			std::vector<u8> sb(10), rb(10);
			std::array< BufferingSocket, 2> ss;

			ss[0].enableLogging();
			ss[1].enableLogging();

			auto task_ = [&](bool sender) -> task<void> {

				MC_BEGIN(task<>, &, sender);
				if (sender)
				{
					MC_AWAIT(ss[0].send(std::move(sb)));

				}
				else
				{
					MC_AWAIT(ss[1].recv(rb));
				}
				MC_END();
			};

			auto t = macoro::make_blocking(macoro::when_all_ready(task_(0), task_(1)));

			BufferingSocket::exchangeMessages(ss[0], ss[1]);

			auto r = t.get();
			std::get<0>(r).result();
			std::get<1>(r).result();
		}

		void BufferingSocket_parSendRecv_test()
		{

			u64 trials = 100;
			i64 numOps = 100;

			macoro::thread_pool ex[4];
			auto work0 = ex[0].make_work();
			auto work1 = ex[1].make_work();
			auto work2 = ex[2].make_work();
			auto work3 = ex[3].make_work();
			ex[0].create_thread();
			ex[1].create_thread();
			ex[2].create_thread();
			ex[3].create_thread();

			for (u64 tt = 0; tt < trials; ++tt)
			{

				std::array<BufferingSocket, 2> s;

				auto f1 = [&](u64 idx) {
					MC_BEGIN(task<void>, idx, &numOps, &s,  
						v = i64{},
						i = i64{},
						buffer = span<u8>{},
						r = std::pair<error_code, u64>{},
						log = std::string{});

					buffer = span<u8>((u8*)&v, sizeof(v));

					//MC_AWAIT(macoro::transfer_to(ex[idx]));

					for (i = 1; i <= numOps; ++i)
					{



						if (idx == 0) {
							MC_RETURN_VOID();
							MC_AWAIT_SET(r, s[0].mSock->recv(buffer));
							COPROTO_ASSERT(v == -i);
						}
						if (idx == 1)
						{
							MC_AWAIT_SET(r, s[1].mSock->recv(buffer));
							log += std::to_string(i) + "\n";
							COPROTO_ASSERT(v == i);
						}
						if (idx == 2)
						{
							v = i;
							MC_AWAIT_SET(r, s[0].mSock->send(buffer));
						}
						if (idx == 3)
						{
							MC_RETURN_VOID();
							v = -i;
							MC_AWAIT_SET(r, s[1].mSock->send(buffer));
						}

						COPROTO_ASSERT(!r.first);
						COPROTO_ASSERT(r.second == sizeof(v));

						//MC_AWAIT(macoro::transfer_to(ex[idx]));
					}
					MC_END();
				};

				std::atomic<bool> done(false);
				auto f = std::thread([&]() {
					while (!done)
						BufferingSocket::exchangeMessages(s[0], s[1]);
					});


				auto t = macoro::sync_wait(macoro::when_all_ready(
					f1(0), f1(1), f1(2), f1(3)
				));

				//using Awaitable = decltype(macoro::when_all_ready(
				//	f1(0), f1(1), f1(2), f1(3)
				//));
				//using R = decltype(macoro::make_blocking<Awaitable>(
				//	std::forward<Awaitable>(std::declval<Awaitable>())).get());

				std::get<0>(t).result();
				std::get<1>(t).result();
				std::get<2>(t).result();
				std::get<3>(t).result();

				done = true;
				f.join();
			}


		}

		void BufferingSocket_cancellation_test()
		{

			//{
			//	std::array<BufferingSocket, 2> s;

			//	//std::vector<u8> sb(10'000'000'000);
			//	auto n = 1'000'000;
			//	u8* sd = new u8[n];
			//	span<u8> sb(sd, n);
			//	sb[4] = 5;
			//	macoro::stop_source src;
			//	auto token = src.get_token();
			//	auto a0 = s[0].mSock->send(sb, token);
			//	std::pair<error_code, u64> res;
			//	//auto a1 = s[1].mSock->recv(rb);
			//	auto b = std::chrono::system_clock::now();
			//	auto task_ = [&](bool sender) -> task<void> {

			//		MC_BEGIN(task<>, &, sender);
			//		if (sender)
			//		{
			//			MC_AWAIT_SET(res, a0);

			//			if (res.second == sb.size())
			//			{
			//				throw std::runtime_error(COPROTO_LOCATION);
			//			}
			//			if (res.first != code::operation_aborted)
			//			{
			//				throw std::runtime_error(COPROTO_LOCATION);
			//			}
			//		}
			//		else
			//		{
			//			src.request_stop();
			//		}

			//		MC_END();
			//	};
			//	
			//	auto t1 = task_(1) | macoro::make_eager();
			//	auto t0 = task_(0) | macoro::make_eager();

			//	BufferingSocket::exchangeMessages(s[0], s[1]);

			//	auto r = macoro::sync_wait(macoro::when_all_ready(std::move(t1), std::move(t0)));
			//	delete[] sd;

			//	std::get<0>(r).result();
			//	std::get<1>(r).result();
			//}

			{

				std::array<BufferingSocket, 2> s;

				std::vector<u8> sb(10);
				sb[4] = 5;
				macoro::stop_source src;
				auto token = src.get_token();
				auto a0 = s[0].mSock->recv(sb, token);
				std::pair<error_code, u64> res;
				//auto a1 = s[1].mSock->recv(rb);
				//auto b = std::chrono::system_clock::now();

				auto task_ = [&](bool sender) -> task<void> {

					MC_BEGIN(task<>, &, sender);
					if (sender)
					{

						MC_AWAIT_SET(res, a0);

						if (res.second)
						{
							throw std::runtime_error(COPROTO_LOCATION);
						}
						if (res.first != code::operation_aborted)
						{
							throw std::runtime_error(COPROTO_LOCATION);
						}
					}
					else
					{
						src.request_stop();
					}

					MC_END();
				};

				auto t = macoro::make_blocking(macoro::when_all_ready(task_(1), task_(0)));

				BufferingSocket::exchangeMessages(s[0], s[1]);

				auto r = t.get();

				std::get<0>(r).result();
				std::get<1>(r).result();
			}

		}


		void BufferingSocket_parCancellation_test()
		{

			u64 trials = 1000;
			u64 numOps = 20;

			macoro::thread_pool ex[4];
			auto work0 = ex[0].make_work();
			auto work1 = ex[1].make_work();
			auto work2 = ex[2].make_work();
			auto work3 = ex[3].make_work();
			ex[0].create_thread();
			ex[1].create_thread();
			ex[2].create_thread();
			ex[3].create_thread();

			for (u64 tt = 0; tt < trials; ++tt)
			{
				std::array<BufferingSocket, 2> s;
				//using Log = std::vector<std::pair<const char*, std::thread::id>>;

				std::vector<std::array<macoro::stop_source, 4>> srcs(numOps + 1);
				std::vector< std::array<macoro::stop_token, 4>> tkns(numOps + 1);

				for (u64 i = 1; i <= numOps; ++i)
				{
					for (auto j = 0ull; j < 4; ++j)
						tkns[i][j] = srcs[i][j].get_token();
				}

				auto f1 = [&](u64 idx) {
					MC_BEGIN(task<void>, idx, &numOps, &s, &srcs, &tkns, &ex,
						v = u64{},
						i = u64{},
						buffer = span<u8>{},
						r = std::pair<error_code, u64>{});

					buffer = span<u8>((u8*)&v, sizeof(v));

					MC_AWAIT(macoro::transfer_to(ex[idx]));
					//MC_AWAIT(ex[idx].post());

					for (i = 1; i <= numOps; ++i)
					{
						if (idx == 0) {
							if (i % 4 == 0)
							{
								srcs[i][3].request_stop();
							}
							else
							{
								MC_AWAIT_SET(r, s[0].mSock->recv(buffer, tkns[i][0]));
								//if (i % 4 == 3)
								//{
								//	COPROTO_ASSERT(r.first == code::operation_aborted);
								//}
								//else
								//{
								//	COPROTO_ASSERT(v == -i);
								//}
							}
						}
						if (idx == 1)
						{
							if (i % 4 == 1)
							{
								srcs[i][2].request_stop();
							}
							else
							{
								MC_AWAIT_SET(r, s[1].mSock->recv(buffer, tkns[i][1]));
								//if (i % 4 == 2)
								//{
								//	COPROTO_ASSERT(r.first == code::operation_aborted);
								//}
								//else
								//{
								//	COPROTO_ASSERT(!r.first && v == i);
								//}
							}
						}
						if (idx == 2)
						{
							if (i % 4 == 2)
							{
								srcs[i][1].request_stop();
							}
							else
							{
								v = i;
								MC_AWAIT_SET(r, s[0].mSock->send(buffer, tkns[i][2]));

								//if (i % 4 == 1)
								//{
								//	COPROTO_ASSERT(r.first == code::operation_aborted);
								//}
							}
						}
						if (idx == 3)
						{

							if (i % 4 == 3)
							{
								srcs[i][0].request_stop();
							}
							else
							{
								v = -i;
								MC_AWAIT_SET(r, s[1].mSock->send(buffer, tkns[i][3]));

								//if (i % 4 == 0)
								//{
								//	COPROTO_ASSERT(r.first == code::operation_aborted);
								//}
							}
						}

						MC_AWAIT(macoro::transfer_to(ex[idx]));
						//MC_AWAIT(ex[0].post());

					}
					MC_END();
				};
				std::atomic<bool> done(false);
				auto f = std::thread([&]() {
					while (!done)
						BufferingSocket::exchangeMessages(s[0], s[1]);
					});


				//auto t0 = macoro::sync_wait();

				auto r = macoro::sync_wait(macoro::when_all_ready(
					f1(0), f1(1), f1(2), f1(3)));
				done = true;
				f.join();

				//auto r = macoro::sync_wait(macoro::when_all_ready(f1(0), f1(1), f1(2), f1(3)));
				try
				{
					std::get<0>(r).result();
					std::get<1>(r).result();
					std::get<2>(r).result();
					std::get<3>(r).result();
				}
				catch (...)
				{

					//w.reset();
					//for (auto& t : thrds)
					//	t.join();
					//throw;
				}
			}



			//w.reset();
			//for (auto& t : thrds)
			//	t.join();
		}

		void BufferingSocket_close_test()
		{

			u64 trials = 100;
			//u64 numOps = 20;

			macoro::thread_pool ex[4];
			auto work0 = ex[0].make_work();
			auto work1 = ex[1].make_work();
			auto work2 = ex[2].make_work();
			auto work3 = ex[3].make_work();
			ex[0].create_thread();
			ex[1].create_thread();
			ex[2].create_thread();
			ex[3].create_thread();


			for (u64 tt = 0; tt < trials; ++tt)
			{

				std::array<BufferingSocket, 2> s;
				auto promise = std::promise<void>{};
				auto fut = promise.get_future().share();

				//using Log = std::vector<std::pair<const char*, std::thread::id>>;

				auto f1 = [&](u64 idx) {
					MC_BEGIN(task<void>, idx, &fut, &s, tt, &ex,
						v = u64{},
						i = u64{},
						buffer = span<u8>{},
						r = std::pair<error_code, u64>{});

					buffer = span<u8>((u8*)&v, sizeof(v));

					MC_AWAIT(transfer_to(ex[idx]));

					fut.get();
					{
						if (tt % 4 == idx)
							MC_AWAIT(s[0].close());

						if (idx == 0) {
							MC_AWAIT_SET(r, s[0].mSock->recv(buffer));
						}
						if (idx == 1)
						{
							MC_AWAIT_SET(r, s[1].mSock->recv(buffer));
						}
						if (idx == 2)
						{
							v = i;
							MC_AWAIT_SET(r, s[0].mSock->send(buffer));
						}
						if (idx == 3)
						{

							v = -i;
							MC_AWAIT_SET(r, s[1].mSock->send(buffer));
						}

						//MC_AWAIT(transfer_to(ex[idx]));
					}

					MC_END();
				};

				std::atomic<bool> done(false);
				auto f = std::thread([&]() {
					while (!done)
						BufferingSocket::exchangeMessages(s[0], s[1]);
					});

				
				promise.set_value();
				macoro::sync_wait(macoro::when_all_ready(
					f1(0), f1(1), f1(2), f1(3)));
				done = true;
				f.join();
			}
		}
	}
}
