#pragma once
#include "coproto/Proto/Operation.h"
#include "coproto/Common/macoro.h"
#include "coproto/Socket/Executor.h"
#include "macoro/stop.h"
#include "coproto/Common/Optional.h"

namespace coproto::internal
{
	struct SocketFork;
	using SocketForkIter = std::list<SocketFork>::iterator;
	u64& recvIndex(SocketFork*);
	// an receive data operation.
	struct RecvOperation
	{

		enum class Status
		{
			NotStarted,
			InProgress,
			//Canceled,
			//Completed
			Canceling
		};

	private:
		SocketFork* mSocketFork;
		coroutine_handle<void> mCH;
		RecvBuffer& mRecvBuffer;

		// the current status of the operation
		Status mStatus = Status::NotStarted;

		optional<macoro::stop_callback> mReg;

		std::vector<std::shared_ptr<FlushToken>> mFlushes;
	public:

		//u64 mIndex;

		RecvOperation(
			RecvBuffer& r,
			coroutine_handle<void> ch,
			SocketForkIter s)
			: mSocketFork(&*s)
			, mCH(ch)
			, mRecvBuffer(r)
			//, mIndex(recvIndex(mSocketFork)++)
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


		//	//, mToken(token)
		//{
		//	// if the user provided a stop token, 
		//	mReg.emplace(mToken, [this] {

		//		CBQueue<coroutine_handle<>> cb;
		//		CBQueue<ExCoHandle> xcb;

		//		macoro::stop_source cancelSrc;
		//		{
		//			auto& s = *mSocketFork->mSched;
		//			Lock l(s.mMutex);
		//			if (mInProgress == false)
		//			{
		//				mRecvBuffer->setError(std::make_exception_ptr(std::system_error(code::operation_aborted)));
		//				COPROTO_ASSERT(mCH);
		//				getCB(cb, xcb, l);
		//				mSocketFork->mRecvOps2_.erase(mSelfIter);

		//				COPROTO_ASSERT(s.mNumRecvs);
		//				--s.mNumRecvs;
		//				if (s.mNumRecvs == 0)
		//				{
		//					s.mRecvStatus = Status::Idle;
		//				}
		//			}
		//			else if (s.mRecvCancelSrc.stop_possible())
		//			{
		//				cancelSrc = std::move(s.mRecvCancelSrc);
		//			}
		//		}

		//		while (xcb)
		//			xcb.pop_front().resume();
		//		while (cb)
		//			cb.pop_front().resume();
		//		if (cancelSrc.stop_possible())
		//			cancelSrc.request_stop();

		//		});
		//}

	//private:

		span<u8> asSpan(u64 size)
		{
			return mRecvBuffer.asSpan(size);
		}

		//void setError(std::exception_ptr ptr)
		//{
		//	mRecvBuffer.setError(std::move(ptr));
		//}

		void setError(error_code ec)
		{
			mRecvBuffer.setError(ec);
			//setError(std::make_exception_ptr(std::system_error(ec)));
		}

		//macoro::stop_token& cancellationToken()
		//{
		//	return mToken;
		//}

		void completeOn(ExecutionQueue::Handle& queue, Lock& l);

	//	// collect the callbacks associated with this operation.
	//	// there is the completion handle and optionally flush operations.
	//	void getCB(ExecutorRef& ref, CBQueue<ExCoHandle>& xcbs, std::unique_lock<std::recursive_mutex>&)
	//	{
	//		xcbs.push_back(ExCoHandle{ ref, std::exchange(mCH, nullptr) });

	//		for (auto& f : mFlushes)
	//		{
	//			if (f.use_count() == 1)
	//			{
	//				xcbs.push_back(ExCoHandle{ ref, std::exchange(f->mHandle, nullptr) });
	//			}
	//		}
	//	}

	//	// collect the callbacks associated with this operation.
	//	// there is the completion handle and optionally flush operations.
	//	void getCB(CBQueue<coroutine_handle<>>& cbs, std::unique_lock<std::recursive_mutex>&)
	//	{
	//		cbs.push_back(std::exchange(mCH, nullptr));

	//		for (auto& f : mFlushes)
	//		{
	//			if (f.use_count() == 1)
	//			{
	//				cbs.push_back(std::exchange(f->mHandle, nullptr));
	//			}
	//		}
	//	}
	};


}