#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "coproto/Common/Defines.h"
#include "coproto/Common/span.h"
#include "coproto/Common/Function.h"

#include "coproto/Common/macoro.h"
#include <vector>
#include <mutex>

namespace coproto
{

	namespace internal
	{
		;
		using Lock = std::unique_lock<std::recursive_mutex>;

		// allows to schedule coro's on an type erased 
		// executor. The call operator on this class
		// forwards the call to scheduler.schedule(h);
		struct ExecutorRef
		{
			void* mScheduler = nullptr;
			function_view<void(void* scheduler, coroutine_handle<>)> mFn = nullptr;

			ExecutorRef() = default;
			ExecutorRef(ExecutorRef&&) = default;
			ExecutorRef& operator=(const ExecutorRef&) = default;

			ExecutorRef(const ExecutorRef& e)
				:mScheduler(e.mScheduler)
				, mFn(e.mFn)
			{
			}
			ExecutorRef(ExecutorRef& e)
				:mScheduler(e.mScheduler)
				, mFn(e.mFn)
			{
			}

			// type erase the scheduler.
			template<typename Scheduler>
			ExecutorRef(Scheduler& s)
				: mScheduler(&s)
				, mFn([](void* s, coroutine_handle<>h) {
				auto& scheduler = *(Scheduler*)s;
				scheduler.schedule(h);
					})
			{}

			// returns true if we have a schedule.
			operator bool() const
			{
				return mScheduler;
			}

			// schedule the coro h.
			void operator()(coroutine_handle<>h)
			{
				mFn(mScheduler, h);
			}
		};

		// an executor paired with a coro handle to
		// be executed on the executor.
		struct ExCoHandle
		{
			ExecutorRef mEx;
			coroutine_handle<> mH;

			void resume()
			{
				mEx.mFn(mEx.mScheduler, mH);
			}
		};

		// a list of callbacks with a small buffer optimization.
		template<typename T>
		struct CBQueue
		{
			CBQueue()
			{
				mVec = mArrayBacking;
			}
			CBQueue(CBQueue&& o)
			{
				*this = std::move(o);
			}

			CBQueue& operator=(CBQueue&& o)
			{
				mHead = (std::exchange(o.mHead, 0));
				mTail = (std::exchange(o.mTail, 0));
				if (o.mVec.data() == o.mArrayBacking.data())
				{
					mArrayBacking = std::move(o.mArrayBacking);
					mVec = mArrayBacking;
				}
				else
				{
					COPROTO_ASSERT(o.mVec.data() == o.mVecBacking.data());
					mVecBacking = std::move(o.mVecBacking);
					mVec = mVecBacking;
				}

				o.mVec = o.mArrayBacking;
				return *this;
			}

			// the head pointer in the ring buffer. not reduced.
			u64 mHead = 0;

			// the tail point in the ring buffer. not reduced.
			u64 mTail = 0;

			const u64 mLogTwoSize = 6;

			// the current active buffer. always a power of 2.
			span<T> mVec;

			// initially the buffer will point to this array
			std::array<T, 8> mArrayBacking;

			// if we overflow mArrayBacking, we will allocate with this.
			// always a power of 2.
			std::vector<T> mVecBacking;

			// add the handle.
			void push_back(T h)
			{
				auto mask = mVec.size() - 1;

				// if full.
				if (mHead == mTail + mVec.size())
				{
					std::vector<T> v(std::max<u64>(mVec.size() * 2, 1ull << mLogTwoSize));
					if (mVec.size())
					{
						auto begin = mTail & mask;
						auto end = mHead & mask;
						for (u64 i = begin, j = 0; i != end; i = (i + 1) & mask, ++j)
						{
							v[j] = std::move(mVec[i]);
						}

						mVecBacking = std::move(v);
						mVec = mVecBacking;
						mHead = mHead - mTail;
						mTail = 0;
					}
				}

				// push back to the next location.
				mVec[mHead++ & mask] = std::move(h);
			}

			u64 size() const { return mHead - mTail; }

			// pops the next item and returns it.
			T pop_front()
			{
				if (size() == 0)
					throw COPROTO_RTE;
				auto mask = mVec.size() - 1;
				return std::move(mVec[mTail++ & mask]);

			}

			operator bool() const
			{
				return size() > 0;
			}

			// run call of the handles, popping them as we go.
			void run()
			{
				while (size())
					pop_front().resume();
			}
		};

		// the default executor is simply the socket itself.
		// when a task is added it is enqued. If no one has 
		// aquaired the executor, then we aquire it and run 
		// all tasks that have been queued, and release it.
		//
		// Its possible that while we have aquired the executor, 
		// more tasks are enqued. We will execute these as well.
		struct ExecutionQueue
		{
			ExecutionQueue()
				:mState(std::make_shared<State>())
			{ }

			ExecutionQueue(const ExecutionQueue&) = delete;
			ExecutionQueue(ExecutionQueue&&) = delete;

		private:
			struct State
			{
				~State()
				{
					if (mHasRunner)
					{
						std::cout << "ExecutionQueue destructed with a runner. terminate is being called." << COPROTO_LOCATION << std::endl;
						std::terminate(); // there is a bug in the program.
					}
				}


				// has someone aquired the exec.
				bool mHasRunner = false;

				// the current list of tasks.
				CBQueue<coroutine_handle<>> mCBs;

				CBQueue<unique_function<void()>> mFns;

				// a mutex that the constructor should provide.
				std::recursive_mutex* mMtx = nullptr;



			};

			std::shared_ptr<State> mState;

		public:

			void setMutex(std::recursive_mutex& mtx)
			{
				mState->mMtx = &mtx;
			}

			// this will try to acquired the callback queues.
			// if it does, it will hold onto them and run all
			// of the queued call backs. If more callbacks
			// are added while we are calling them, these
			// will also be run.
			struct Handle
			{
				std::shared_ptr<State> mEx = nullptr;
				bool mAquired = false;

				CBQueue<ExCoHandle> mXCBs;
				CBQueue<coroutine_handle<>> mCBs;
				CBQueue<unique_function<void()>> mFns;

				~Handle()
				{
					assert(mXCBs.size() == 0);
				}

				Handle() = default;
				Handle(Handle&&) = default;
				Handle& operator=(Handle&&) = default;

				// check if no one else calling the callbacks and if so
				// take the callbacks.
				Handle(std::shared_ptr<State> e, Lock& lock)
				{
					mEx = std::move(e);
					assert(mEx->mMtx == lock.mutex());
					//assert(l.mutex() == mEx->mMtx);
					if (mEx->mHasRunner == false)
					{
						mEx->mHasRunner = true;
						mAquired = true;
						//mCBs = std::move(mEx->mCBs);
						//mFns = std::move(mEx->mFns);
					}
				}

				void push_back_fn(unique_function<void()> f, Lock& lock)
				{
					assert(mEx->mMtx == lock.mutex());
					if(mAquired)
						mFns.push_back(std::move(f));
					else
						mEx->mFns.push_back(std::move(f));
				}

				void push_back(std::coroutine_handle<> h, ExecutorRef ref, Lock& lock)
				{
					push_back(coroutine_handle<>(h), ref, lock);
				}
				void push_back(coroutine_handle<> h, ExecutorRef ref, Lock& lock)
				{
					assert(mEx->mMtx == lock.mutex());
					if (ref)
						mXCBs.push_back({ ref, h });
					else
					{
						if(mAquired)
							mCBs.push_back(h);
						else
							mEx->mCBs.push_back(h);
					}
				}


				// run all of the callbacks that were acquired. 
				// once those are run, check if more have been added
				// and if so run those. repeat.
				MACORO_NODISCARD
					coroutine_handle<> runReturnLast()
				{
					while (mXCBs.size())
						mXCBs.pop_front().resume();

					assert(mEx->mMtx);
					coroutine_handle<> next = nullptr;
					while (mAquired)
					{
						if(mCBs.size() == 0 && mFns.size() == 0)
						{
							std::unique_lock<std::recursive_mutex> l(*mEx->mMtx);
							std::swap(mEx->mCBs, mCBs);
							std::swap(mEx->mFns, mFns);
							if (mCBs.size() == 0 && mFns.size() == 0)
							{
								mEx->mHasRunner = false;
								mAquired = false;
							}
						}

						while (mCBs.size() || mFns.size())
						{
							if (mFns.size())
							{
								mFns.pop_front()();
							}
							else
							{
								auto h = mCBs.pop_front();

								if (next)
									next.resume();

								next = h;
							}

							//if (mCBs.size() == 0 && mFns.size() == 0)
							//{
							//	std::unique_lock<std::recursive_mutex> l(*mEx->mMtx);
							//	std::swap(mEx->mCBs, mCBs);
							//	std::swap(mEx->mFns, mFns);
							//	if (mCBs.size() == 0 && mFns.size() == 0)
							//	{
							//		mEx->mHasRunner = false;
							//		mAquired = false;
							//	}
							//}
						}
					}

					if (next == nullptr)
						next = macoro::noop_coroutine();

					return next;
				}

				void run()
				{
					runReturnLast().resume();
				}
			};

			Handle acquire(std::unique_lock<std::recursive_mutex>& l)
			{
				assert(mState->mMtx == nullptr || mState->mMtx == l.mutex());
				return Handle(mState, l);
			}

		};

	}
}