#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.



#include "TaskProto_tests.h"
#include "coproto/Socket/Socket.h"
#include "coproto/Socket/LocalAsyncSock.h"
#include "coproto/Socket/BufferingSocket.h"
#include "macoro/sync_wait.h"
#include "macoro/start_on.h"
#include <numeric>
#include "macoro/inline_scheduler.h"
#include "macoro/thread_pool.h"
namespace coproto
{

	namespace tests
	{
		enum EvalTypes
		{
			async, Buffering
		};

		template<typename P0, typename P1>
		auto evalEx(P0&& p0, P1&& p1, EvalTypes type)
		{
			if (type == EvalTypes::async)
			{

				macoro::thread_pool stx0, stx1;
				auto w0 = stx0.make_work();
				auto w1 = stx1.make_work();
				stx0.create_threads(4);
				stx1.create_threads(4);
				auto s = LocalAsyncSocket::makePair();

				auto w = macoro::when_all_ready(p0(s[0], 0, stx0), p1(s[1], 1, stx1));
				auto r = macoro::sync_wait(std::move(w));
				s[0].close();
				s[1].close();
				return r;
			}
			else
			{
				throw COPROTO_RTE;
				//std::array<BufferingSocket, 2> s;

				//macoro::thread_pool ex0, ex1;
				//auto w0 = ex0.make_work();
				//auto w1 = ex1.make_work();
				//ex0.create_threads(1);
				//ex1.create_threads(1);

				//std::atomic<int> done = 2;

				//auto F = [&](int i)
				//{
				//	MC_BEGIN(task)
				//}

				//auto w = macoro::when_all_ready(
				//	p0(s[0], 0, ex0) | macoro::then([](){ --done; }) | macoro::start_on(ex0),
				//	p1(s[1], 1, ex1) | macoro::then([](){ --done; }) | macoro::start_on(ex1));

				//auto thrd = std::thread([&]() {
				//	while (!done)
				//		BufferingSocket::exchangeMessages(s[0], s[1]);
				//	});

				//auto r = macoro::sync_wait(std::move(w));

				//return r;
			}
		}


		template<typename P0>
		auto evalEx(P0&& p0, EvalTypes type)
		{
			return evalEx(p0, p0, type);
		}

		template<typename P0, typename P1>
		auto eval(P0&& p0, P1&& p1, EvalTypes type)
		{
			if (type == EvalTypes::async)
			{
				auto s = LocalAsyncSocket::makePair();
				auto w = macoro::when_all_ready(p0(s[0], 0), p1(s[1], 1));
				auto r = macoro::sync_wait(std::move(w));
				return r;
			}
			else
			{

				std::array<BufferingSocket, 2> s;
				
				auto t0 = macoro::when_all_ready(
					p0(s[0], 0), p0(s[1], 1))
					| macoro::make_blocking();

				BufferingSocket::exchangeMessages(s[0], s[1]);

				auto r = t0.get();

				return r;
			}
		}


		template<typename P0>
		auto eval(P0&& p0, EvalTypes type)
		{
			return eval(p0, p0, type);
		}
	}
}