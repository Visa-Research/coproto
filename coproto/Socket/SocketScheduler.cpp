#include "SocketScheduler.h"
#include "macoro/wrap.h"

#ifdef COPROTO_SOCK_LOGGING
#define RECV_LOG(X) if(mLogging) mRecvLog.push_back(X)
#define SEND_LOG(X) if(mLogging) mSendLog.push_back(X)
#else
#define RECV_LOG(X)
#define SEND_LOG(X)
#endif

namespace coproto {
	namespace internal
	{
		coroutine_handle<void> SockScheduler::recv(SessionID id, RecvBuffer* data, coroutine_handle<void> ch, macoro::stop_token&& token)
		{
			ExecutionQueue::Handle exQueue;
			{
				Lock l = Lock(mMutex);
				exQueue = mExQueue.acquire(l);

				auto fork = getLocalSocketFork(id, l);

				if (mEC)
				{
					data->setError(mEC);
					exQueue.push_back(ch, fork->mExecutor, l);
				}
				else
				{
					++mNumRecvs;
					auto& op = fork->emplace_recv(l, *data, ch, fork);

					// if this is only recv op, we need to resume the recv task
					if (mAnyRecvOp)
					{
						COPROTO_ASSERT(mRecvStatus == Status::Idle);
						mRecvStatus = Status::InUse;
						op.setStatus(RecvOperation::Status::InProgress);
						exQueue.push_back(mAnyRecvOp->getHandle(code::success, mAnyRecvOp), fork->mExecutor, l);
					}

					// if the recv task was wanting this specific recv op,
					// we need to resume it.
					if (mGetRequestedRecvSocketFork &&  
						fork->mRemoteId == mGetRequestedRecvSocketFork->forkID())
					{
						COPROTO_ASSERT(mRecvStatus == Status::RequestedRecvOp);
						mRecvStatus = Status::InUse;
						op.setStatus(RecvOperation::Status::InProgress);
						exQueue.push_back(mGetRequestedRecvSocketFork->getHandle(macoro::Ok(&op), mGetRequestedRecvSocketFork), fork->mExecutor, l);
					}

					// install the cancelation handle. This must be 
					// done after the operation has [possible] started 
					// to ensure that we cancel it correctly. In particular, 
					// cancel might be called from some other thread at any time.
					op.setCancelation(std::move(token), [this, opPtr = &op]() {
						macoro::stop_source cancelSrc;
						ExecutionQueue::Handle exQueue;
						{
							Lock l(mMutex);
							exQueue = mExQueue.acquire(l);
							if (opPtr->status() == RecvOperation::Status::NotStarted)
							{
								// we will skip this operation
								--mNumRecvs;
								opPtr->setError(code::operation_aborted);
								opPtr->completeOn(exQueue, l);
								opPtr->fork().erase_recv(l, opPtr);
							}
							else
							{
								COPROTO_ASSERT(opPtr->status() == RecvOperation::Status::InProgress);
								opPtr->setStatus(RecvOperation::Status::Canceling);
								// the operation is in progress, call cancel.
								if (cancelSrc.stop_possible())
									exQueue.push_back_fn([cancelSrc = std::move(mRecvCancelSrc)]() mutable {
									cancelSrc.request_stop();
										}, l);
							}
						}
						exQueue.run();

						});
				}
			}

			return exQueue.runReturnLast();
		}

		SessionID SockScheduler::fork(SessionID s)
		{
			Lock l(mMutex);
			auto slot = getLocalSocketFork(s, l);
			auto s2 = slot->mSessionID.derive();
			initLocalSocketFork(s2, slot->mExecutor, l);
			return s2;
		}

		void SockScheduler::initLocalSocketFork(const SessionID& id, const ExecutorRef& ex, Lock& _)
		{
			auto iter = mIdSocketForkMapping_.find(id);
			if (iter == mIdSocketForkMapping_.end())
			{
				// We have initialized the slot before receiving any messages.
				mSocketForks_.emplace_back(id);
				auto slot = --mSocketForks_.end();
				iter = mIdSocketForkMapping_.insert({ id, slot }).first;
			}
			else
			{
				// We have already received a message for this slot.
				// assign its local id.
				COPROTO_ASSERT(~iter->second->mLocalId == 0);
			}

			iter->second->mLocalId = mNextLocalSocketFork++;
			iter->second->mExecutor = ex;
		}

		SocketForkIter SockScheduler::getLocalSocketFork(const SessionID& id, Lock& _)
		{
			auto iter = mIdSocketForkMapping_.find(id);
			COPROTO_ASSERT(iter != mIdSocketForkMapping_.end() && iter->second->mLocalId != 0);
			return iter->second;
		}

		error_code SockScheduler::initRemoteSocketFork(u32 slotId, SessionID id, Lock& _)
		{
			if (slotId == ~u32(0))
				return code::badCoprotoMessageHeader;

			auto iter = mIdSocketForkMapping_.find(id);
			SocketForkIter slot;
			if (iter == mIdSocketForkMapping_.end())
			{
				mSocketForks_.emplace_back(id);
				slot = --mSocketForks_.end();
				iter = mIdSocketForkMapping_.insert({ id, slot }).first;

			}
			else
				slot = iter->second;

			if (slot->mRemoteId != ~u32(0))
				return code::badCoprotoMessageHeader;

			COPROTO_ASSERT(slot->mSessionID == id);
			COPROTO_ASSERT(mRemoteSocketForkMapping_.find(slotId) == mRemoteSocketForkMapping_.end());
			slot->mRemoteId = slotId;
			mRemoteSocketForkMapping_.insert({ slotId, slot });

			return {};
		}

		coroutine_handle<> SockScheduler::flush(coroutine_handle<> h)
		{
			Lock l(mMutex);
			if (mNumRecvs == 0 && mSendBufferBegin == nullptr)
				return h;

			auto f = std::make_shared<FlushToken>(h);

			for (auto& slot : mSocketForks_)
			{
				if (slot.size_recv(l))
					slot.back_recv(l).addFlush(f, l);
				if (slot.size_send(l))
					slot.back_send(l).addFlush(f, l); 
			}

			return macoro::noop_coroutine();
		}

		void SockScheduler::cancel(
			ExecutionQueue::Handle& queue,
			Caller c,
			error_code ec,
			Lock& l)
		{

			if (!mEC)
			{
				COPROTO_ASSERT(ec);
				mEC = ec;
				if (mRecvStatus == Status::InUse)
					mRecvCancelSrc.request_stop();
				if (mSendStatus == Status::InUse)
					mSendCancelSrc.request_stop();
				//COPROTO_ASSERT(mCloseSock);

				//queue.push_back_fn([f = std::move(mCloseSock)]() mutable {
				//	f();
				//	}, l);

				if (mAnyRecvOp)
				{
					queue.push_back(mAnyRecvOp->getHandle(code::cancel, mAnyRecvOp), {}, l);
				}

				if (mGetRequestedRecvSocketFork)
				{
					auto e = (c == Caller::Sender) ?
						error_code(code::cancel) :
						std::exchange(ec, code::cancel);
					queue.push_back(mGetRequestedRecvSocketFork->getHandle(macoro::Err(e), mGetRequestedRecvSocketFork), {}, l);
				}

				if (mNextSendOp)
				{
					queue.push_back(mNextSendOp->getHandle(macoro::Err(error_code(code::cancel)), mNextSendOp), {}, l);
				}
			}

			if (c == Caller::Sender)
			{
				mSendStatus = Status::Closed;
				auto iter = mSendBufferBegin;
				mSendBufferBegin = nullptr;
				mSendBufferLast = nullptr;
				SEND_LOG("close");

				while (iter)
				{
					auto& op = *iter;
					assert(op.status() == SendOperation::Status::NotStarted);
					assert(&op == &op.fork().front_send(l));

					op.setError(std::exchange(ec, code::cancel));
					op.completeOn(queue, l);
					iter = op.next();
					op.fork().pop_front_send(l);
				}
			}
			else
			{
				SEND_LOG("try-close");
			}

			if (c == Caller::Recver)
			{
				RECV_LOG("close");
				mRecvStatus = Status::Closed;
				for (auto& fork : mSocketForks_)
				{
					while (fork.size_recv(l))
					{
						auto& op = fork.front_recv(l);
						op.setError(std::exchange(ec, code::cancel));
						op.completeOn(queue, l);
						fork.pop_front_recv(l);
					}
				}
				mNumRecvs = 0;
			}
			else
			{
				RECV_LOG("try-close");
			}
		}

		void SockScheduler::close()
		{
			ExecutionQueue::Handle queue;
			{
				Lock l(mMutex);
				queue = mExQueue.acquire(l);

				cancel(queue, Caller::Extern, code::closed, l);
			}

			queue.run();
		}
	}
}
