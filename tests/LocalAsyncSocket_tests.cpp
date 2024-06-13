#include "LocalAsyncSocket_tests.h"
#include "coproto/Socket/LocalAsyncSock.h"
#include "macoro/transfer_to.h"
#include <future>
#include "macoro/macros.h"
#include "macoro/thread_pool.h"


void coproto::tests::LocalAsyncSocket_noop_test()
{
	auto s = LocalAsyncSocket::makePair();
}

void coproto::tests::LocalAsyncSocket_sendRecv_test()
{
	auto s = LocalAsyncSocket::makePair();
	s[0].enableLogging();
	s[1].enableLogging();

	u64 count = 0;
	auto task_ = [&]() -> task<void> {

		MC_BEGIN(task<>, &);
		count++;
		MC_AWAIT(macoro::suspend_always{});
		std::cout << "failed " << COPROTO_LOCATION << std::endl;
		std::terminate();
		MC_END();
		};

	std::vector<u8> c(10);
	auto a0 = s[0].mSock->send(c);
	COPROTO_ASSERT(a0.await_ready() == false);
	auto t0 = task_();
	auto h0 = a0.await_suspend(t0.handle());
	(void)h0;

	COPROTO_ASSERT(h0 != t0.handle());
	COPROTO_ASSERT(count == 0);

	auto a1 = s[1].mSock->recv(c);
	COPROTO_ASSERT(a1.await_ready() == false);
	auto t1 = task_();
	a1.await_suspend(t1.handle()).resume();

	COPROTO_ASSERT(count == 2);
	//COPROTO_ASSERT(count == 1);
	//COPROTO_ASSERT(h1 == t1.handle());
}

void coproto::tests::LocalAsyncSocket_parSendRecv_test()
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

		auto s = LocalAsyncSocket::makePair();

		auto f1 = [&](u64 idx) {
			MC_BEGIN(task<void>, &ex, idx, &numOps, &s,
				v = u64{},
				i = u64{},
				buffer = span<u8>{},
				r = std::pair<error_code, u64>{});

			buffer = span<u8>((u8*)&v, sizeof(v));

			MC_AWAIT(macoro::transfer_to(ex[idx]));

			for (i = 1; i <= numOps; ++i)
			{

				if (idx == 0) {
					MC_AWAIT_SET(r, s[0].mSock->recv(buffer));
					COPROTO_ASSERT(v == -i);
				}
				if (idx == 1)
				{
					MC_AWAIT_SET(r, s[1].mSock->recv(buffer));
					COPROTO_ASSERT(v == i);
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

				COPROTO_ASSERT(!r.first);
				COPROTO_ASSERT(r.second == sizeof(v));

				MC_AWAIT(macoro::transfer_to(ex[idx]));
			}
			MC_END();
			};

		macoro::sync_wait(macoro::when_all_ready(f1(0), f1(1), f1(2), f1(3)));
	}
}

void coproto::tests::LocalAsyncSocket_cancellation_test()
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
		auto s = LocalAsyncSocket::makePair();

		//using Log = std::vector<std::pair<const char*, std::thread::id>>;

		std::vector<std::array<macoro::stop_source, 4>> srcs(numOps + 1);
		std::vector< std::array<macoro::stop_token, 4>> tkns(numOps + 1);

		for (u64 i = 1; i <= numOps; ++i)
		{
			for (auto j = 0ull; j < 4; ++j)
				tkns[i][j] = srcs[i][j].get_token();
		}

		auto f1 = [&](u64 idx) {
			MC_BEGIN(task<void>, &ex, idx, &numOps, &s, &srcs, &tkns,
				v = u64{},
				i = u64{},
				buffer = span<u8>{},
				r = std::pair<error_code, u64>{});

			buffer = span<u8>((u8*)&v, sizeof(v));

			MC_AWAIT(macoro::transfer_to(ex[idx]));
			//MC_AWAIT(ex[0].post());

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
						if (i % 4 == 3)
						{
							COPROTO_ASSERT(r.first == code::operation_aborted);
						}
						else
						{
							COPROTO_ASSERT(v == -i);
						}
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
						if (i % 4 == 2)
						{
							COPROTO_ASSERT(r.first == code::operation_aborted);
						}
						else
						{
							COPROTO_ASSERT(!r.first && v == i);
						}
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

						if (i % 4 == 1)
						{
							COPROTO_ASSERT(r.first == code::operation_aborted);
						}
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

						if (i % 4 == 0)
						{
							COPROTO_ASSERT(r.first == code::operation_aborted);
						}
					}
				}

				MC_AWAIT(macoro::transfer_to(ex[idx]));
				//MC_AWAIT(ex[0].post());

			}
			MC_END();
			};
		macoro::sync_wait(macoro::when_all_ready(f1(0), f1(1), f1(2), f1(3)));
	}
}

void coproto::tests::LocalAsyncSocket_close_test()
{

	u64 trials = 2000;
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

		auto s = LocalAsyncSocket::makePair();
		auto promise = std::promise<void>{};
		auto fut = promise.get_future().share();
		std::array<std::promise<void>, 4> proms;
		std::array<std::future<void>, 4> futures{
			proms[0].get_future(),
			proms[1].get_future(),
			proms[2].get_future(),
			proms[3].get_future()
		};
		//using Log = std::vector<std::pair<const char*, std::thread::id>>;

		auto f1 = [&](u64 idx) -> task<> {
			//MC_BEGIN(task<void>, &ex, idx, &proms, &fut, &s,tt,
			auto v = u64{};
			auto i = u64{};
			auto buffer = span<u8>{};
			auto r = std::pair<error_code, u64>{};

			buffer = span<u8>((u8*)&v, sizeof(v));

			co_await(macoro::transfer_to(ex[idx]));
			//MC_AWAIT(ex[0].post());
			proms[idx].set_value();
			fut.get();
			{
				if (tt % 4 == idx)
					co_await(s[0].close());

				if (idx == 0) {
					r = co_await s[0].mSock->recv(buffer);
				}
				if (idx == 1)
				{
					r = co_await s[1].mSock->recv(buffer);
				}
				if (idx == 2)
				{
					v = i;
					r = co_await s[0].mSock->send(buffer);
				}
				if (idx == 3)
				{

					v = -i;
					r = co_await s[1].mSock->send(buffer);
				}

				co_await(macoro::transfer_to(ex[idx]));
				//MC_AWAIT(ex[0].post());
			}

			//MC_END();
			};

		//auto t0 = f1(0) | macoro::make_eager();
		//auto t1 = f1(1) | macoro::make_eager();
		//auto t2 = f1(2) | macoro::make_eager();
		//auto t3 = f1(3) | macoro::make_eager();
		auto t = macoro::make_blocking(macoro::when_all_ready(
			f1(0),
			f1(1),
			f1(2),
			f1(3)
		));

		promise.set_value();
		t.get();
	}

}
