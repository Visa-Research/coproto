#pragma once

#include <list>
#include "coproto/Common/macoro.h"
//#include "coproto/Proto/Buffers.h"
#include "coproto/Socket/Executor.h"
#include "macoro/stop.h"
#include "coproto/Common/Optional.h"
#include "coproto/Common/InlinePoly.h"
#include "coproto/Proto/Operation.h"

namespace coproto::internal
{


	struct SockScheduler;
	struct SocketFork;
	using SocketForkIter = std::list<SocketFork>::iterator;


	struct SendOperation
	{

		enum class Status
		{
			NotStarted,
			InProgress,
			Canceling
		};

	private:
		// the completion handle
		coroutine_handle<void> mCH = nullptr;

		// optional storage to keep the buffer alive
		InlinePoly<SendBuffer, 8 * sizeof(u64)> mStorage;

		// the data to be sent.
		span<u8> mSendBuffer;

		// the current status of the operation
		Status mStatus = Status::NotStarted;

		// a pointer to the parent fork.
		SocketForkIter mSocketFork;

		// in intrusive linked list of the send operation in order.
		SendOperation* mNext = nullptr;

		// in intrusive linked list of the send operation in order.
		SendOperation* mPrev = nullptr;

		// an optional stop token assoicated with this operation.
		//macoro::stop_token mToken;

		// if mToken is set, then this will be the registation callback.
		optional<macoro::stop_callback> mReg;

		// flush operations that are waiting on this operation.
		std::vector<std::shared_ptr<FlushToken>> mFlushes;
	public:

		SendOperation() = delete;
		SendOperation(const SendOperation&) = delete;
		SendOperation(SendOperation&&) = delete;

		template<typename Buffer>
		SendOperation(
			SocketForkIter ss,
			coroutine_handle<void> ch,
			Buffer&& s)
			: mCH(ch)
			, mStorage(std::forward<Buffer>(s))
			, mSendBuffer(mStorage->asSpan())
			, mSocketFork(ss)
		{}

		template<typename Fn>
		void setCancelation(
			macoro::stop_token&& token,
			Fn&& cancelationFn)
		{
			if (token.stop_possible())
			{
				mReg.emplace(std::move(token), std::move(cancelationFn));
			}
		}

		void addFlush(std::shared_ptr<FlushToken>& t, Lock& l)
		{
			mFlushes.push_back(t);
		}

		SocketFork& fork()
		{
			return *mSocketFork;
		}

		Status status() 
		{
			return mStatus;
		}

		void setStatus(Status s)
		{
			mStatus = s;
		}

		SendOperation* next()
		{
			return mNext;
		}
		SendOperation* prev()
		{
			return mPrev;
		}


		void setNext(SendOperation* next)
		{
			mNext = next;
			if(next)
				next->mPrev = this;
		}

		//[this] {

		//	CBQueue<coroutine_handle<>> cb;
		//	CBQueue<ExCoHandle> xcb;
		//	macoro::stop_source cancelSrc;
		//	{
		//		//auto& s = *mSocketFork->mSched;
		//		Lock l(s.mMutex);
		//		if (mInProgress == false)
		//		{
		//			mSendBuff.setError(std::make_exception_ptr(std::system_error(code::operation_aborted)));
		//			COPROTO_ASSERT(mCH);
		//			getCB(cb, xcb, l);

		//			auto& queue = scheduler.mSendBuffers_;
		//			auto iter = std::find(queue.begin(), queue.end(), mSocketFork);
		//			COPROTO_ASSERT(iter != queue.end());
		//			queue.erase(iter);

		//			mSocketFork->mSendOps2_.erase(mSelfIter);

		//			if (queue.size() == 0)
		//			{
		//				s.mSendStatus = Status::Idle;
		//			}
		//		}
		//		else if (s.mSendCancelSrc.stop_possible())
		//		{
		//			cancelSrc = std::move(s.mSendCancelSrc);
		//		}
		//	}

		//	while (xcb)
		//		xcb.pop_front().resume();
		//	while (cb)
		//		cb.pop_front().resume();
		//	if (cancelSrc.stop_possible())
		//		cancelSrc.request_stop();
		//	}

		span<u8> asSpan()
		{
			return mStorage->asSpan();
		}

		//void setError(std::exception_ptr ptr)
		//{
		//	mStorage->setError(std::move(ptr));
		//}

		void setError(error_code ec)
		{
			mStorage->setError(ec);
		}

		//macoro::stop_token& cancellationToken()
		//{
		//	return mToken;
		//}

		void completeOn(ExecutionQueue::Handle& queue, Lock& l);

		//// collect the callbacks associated with this operation.
		//// there is the completion handle and optionally flush operations.
		//void getCB(ExecutorRef& ex, CBQueue<ExCoHandle>& xcbs, Lock&)
		//{
		//	xcbs.push_back(ExCoHandle{ ex, std::exchange(mCH, nullptr) });

		//	for (auto& f : mFlushes)
		//	{
		//		if (f.use_count() == 1)
		//		{
		//			xcbs.push_back(ExCoHandle{ ex, std::exchange(f->mHandle, nullptr) });
		//		}
		//	}
		//}

		//// collect the callbacks associated with this operation.
		//// there is the completion handle and optionally flush operations.
		//void getCB(CBQueue<coroutine_handle<>>& cbs, Lock&)
		//{
		//	cbs.push_back(std::exchange(mCH, nullptr));

		//	for (auto& f : mFlushes)
		//	{
		//		if (f.use_count() == 1)
		//		{
		//			cbs.push_back(std::exchange(f->mHandle, nullptr));

		//		}
		//	}
		//}
	};

}