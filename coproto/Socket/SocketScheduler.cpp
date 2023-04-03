#include "SocketScheduler.h"
#include "macoro/wrap.h"
namespace coproto {
	namespace internal
	{
		coroutine_handle<void> SockScheduler::recv(SessionID id, RecvBuffer* data, coroutine_handle<void> ch, macoro::stop_token&& token)
		{

			bool acquired = false;
			error_code ec;
			ExecutorRef exec;
			{
				Lock l = Lock(mMutex);
				auto slot = getLocalSlot(id, l);
				exec = slot->mExecutor;

				if (mEC)
					ec = mEC;
				else if (token.stop_requested())
					ec = code::operation_aborted;
				else
				{

					if (slot->mClosed)
						ec = code::closed;
					else
					{
						slot->mRecvOps2_.emplace_back(data, ch, slot, std::move(token));
						auto& op = slot->mRecvOps2_.back();
						op.mSelfIter = --slot->mRecvOps2_.end();
						++mNumRecvs;

						if (mRecvStatus == Status::Idle)
						{
#ifdef COPROTO_SOCK_LOGGING
							mRecvLog.push_back("recv::resume");
#endif
							acquired = true;
						}
						if (mRecvStatus == Status::RequestedRecvOp &&
							slot->mRemoteId == mRecvHeader[1])
						{
#ifdef COPROTO_SOCK_LOGGING
							mRecvLog.push_back("recv::resume-requested");
#endif
							assert(mRequestedRecvSlot == slot);
							slot->mRecvOps2_.front().mInProgress = true;
							acquired = true;
						}
						if (acquired)
							mRecvStatus = Status::InUse;
					}
				}
			}

			if (ec)
			{
				data->setError(std::make_exception_ptr(std::system_error(ec)));
				if (exec)
				{
					exec(ch);
					ch = macoro::noop_coroutine();
				}
				return ch;
			}
			else if (acquired)
			{
				COPROTO_ASSERT(mRecvTaskHandle);
				return std::exchange(mRecvTaskHandle, nullptr);
			}
			else
				return macoro::noop_coroutine();
		}


		coroutine_handle<void> SockScheduler::send(
			SessionID id,
			SendBuffer&& op,
			coroutine_handle<void> callback,
			macoro::stop_token&& token,
			bool async)
		{
			assert(callback);
			if (op.asSpan().size() == 0)
			{
				op.setError(std::make_exception_ptr(std::system_error(code::sendLengthZeroMsg)));
				return callback;
			}

			bool acquired = false;
			error_code ec;
			SlotIter slot;
			ExecutorRef exec;
			{
				Lock l = Lock(mMutex);
				slot = getLocalSlot(id, l);
				exec = slot->mExecutor;

				if (mEC)
					ec = mEC;
				else if (token.stop_requested())
					ec = code::operation_aborted;
				else
				{

					if (slot->mClosed)
						ec = code::closed;
					else
					{
						acquired = mSendStatus == Status::Idle;
						if (acquired)
							mSendStatus = Status::InUse;

						auto cb = (async)?
							macoro::noop_coroutine() :
							std::exchange(callback, macoro::noop_coroutine());

						slot->mSendOps2_.emplace_back(slot, cb, std::move(op), std::move(token));

						auto& op = slot->mSendOps2_.back();
						op.mSelfIter = --slot->mSendOps2_.end();
						if (mSendBuffers_.size() == 0)
							op.mInProgress = true;
						mSendBuffers_.push_back(slot);
					}
				}
			}

			if (ec)
				op.setError(std::make_exception_ptr(std::system_error(ec)));

			if (acquired)
			{
				// we need to resume mSendTaskHandle. We have two cases
				// 1) callback engaged: we want to resume both callback and mSendTaskHandle
				// 2) callback disengaged: we want to resume mSendTaskHandle
				// 
				// For 1, if we have an executor then we will resume callback using that.
				COPROTO_ASSERT(mSendTaskHandle);
				if (callback != macoro::noop_coroutine())
				{
					assert(async);
					if (exec)
					{
						exec(callback);
						return std::exchange(mSendTaskHandle, nullptr);
					}
					else
					{
						std::exchange(mSendTaskHandle, nullptr).resume();
						return callback;
					}
				}
				else 
				{
					return std::exchange(mSendTaskHandle, nullptr);
				}
			}
			else
			{
				// if we have an executor, then resume on that. Otherwise
				// just return callback.
				if (callback != macoro::noop_coroutine() && exec)
				{
					exec(callback);
					callback = macoro::noop_coroutine();
				}

				return callback;
			}
		}

		SessionID SockScheduler::fork(SessionID s)
		{
			Lock l(mMutex);
			auto slot = getLocalSlot(s, l);
			auto s2 = slot->mSessionID.derive();
			initLocalSlot(s2, slot->mExecutor, l);
			return s2;
		}


		u32& SockScheduler::getSendHeaderSlot()
		{
			return mSendHeader[1];
		}
		u32& SockScheduler::getSendHeaderSize()
		{
			return mSendHeader[0];
		}
		span<u8> SockScheduler::getSendHeader()
		{
			return span<u8>((u8*)&mSendHeader, sizeof(mSendHeader));
		}
		optional<SockScheduler::SlotIter> SockScheduler::getRecvHeaderSlot(Lock& _)
		{
			auto theirSlot = mRecvHeader[1];
			auto iter = mRemoteSlotMapping_.find(theirSlot);
			if (iter == mRemoteSlotMapping_.end())
			{
				return {};
			}
			return iter->second;
		}
		u32 SockScheduler::getRecvHeaderSize()
		{
			return mRecvHeader[0];
		}

		span<u8> SockScheduler::getRecvHeader()
		{
			return span<u8>((u8*)&mRecvHeader, sizeof(mRecvHeader));
		}

		span<u8> SockScheduler::getSendCtrlBlk()
		{
			return span<u8>((u8*)&mSendControlBlock, sizeof(mSendControlBlock));
		}

		span<u8> SockScheduler::getSendCtrlBlk2()
		{
			return span<u8>((u8*)&mSendControlBlock2, sizeof(mSendControlBlock2));
		}



		void SockScheduler::initLocalSlot(const SessionID& id, const ExecutorRef& ex, Lock& _)
		{
			auto iter = mIdSlotMapping_.find(id);
			if (iter == mIdSlotMapping_.end())
			{
				// We have initialized the slot before receiving any messages.
				mSlots_.emplace_back(this);
				auto slot = --mSlots_.end();
				slot->mSessionID = id;
				iter = mIdSlotMapping_.insert({ id, slot }).first;
			}
			else
			{
				// We have already received a message for this slot.
				// assign its local id.
				COPROTO_ASSERT(~iter->second->mLocalId == 0);
			}

			iter->second->mLocalId = mNextLocalSlot++;
			iter->second->mExecutor = ex;
		}

		SockScheduler::SlotIter SockScheduler::getLocalSlot(const SessionID& id, Lock& _)
		{
			auto iter = mIdSlotMapping_.find(id);
			COPROTO_ASSERT(iter != mIdSlotMapping_.end() && iter->second->mLocalId != 0);
			return iter->second;
		}

		error_code SockScheduler::initRemoteSlot(u32 slotId, SessionID id, Lock& _)
		{
			if (slotId == ~u32(0))
				return code::badCoprotoMessageHeader;

			auto iter = mIdSlotMapping_.find(id);
			SlotIter slot;
			if (iter == mIdSlotMapping_.end())
			{
				mSlots_.emplace_back(this);
				slot = --mSlots_.end();
				slot->mSessionID = id;
				iter = mIdSlotMapping_.insert({ id, slot }).first;

			}
			else
				slot = iter->second;

			if (slot->mRemoteId != ~u32(0))
				return code::badCoprotoMessageHeader;

			COPROTO_ASSERT(slot->mSessionID == id);
			//COPROTO_ASSERT(slot->mRemoteId == ~u32(0));
			COPROTO_ASSERT(mRemoteSlotMapping_.find(slotId) == mRemoteSlotMapping_.end());
			slot->mRemoteId = slotId;
			mRemoteSlotMapping_.insert({ slotId, slot });

			return {};
		}


		void SockScheduler::closeFork(SessionID sid)
		{
			assert(0 && "not impl");
			CBList cbs;
			SlotIter slot;
			bool clearSend = false, clearRecv = false;
			{
				Lock l(mMutex);

				slot = getLocalSlot(sid, l);
				slot->mClosed = true;

				if (slot->mSendOps2_.size())
				{
					if (slot->mSendOps2_.front().mInProgress == false)
					{
						for (auto& op : slot->mSendOps2_)
						{
							op.mSendBuff.setError(std::make_exception_ptr(std::system_error(code::operation_aborted)));
							op.getCB(cbs, l);
						}
						clearSend = true;
					}
					else
					{
						auto src = std::move(mSendCancelSrc);
						if (src.stop_possible())
							src.request_stop();
					}
				}

				if (slot->mRecvOps2_.size())
				{
					if (slot->mRecvOps2_.front().mInProgress == false)
					{
						for (auto& op : slot->mRecvOps2_)
						{
							op.mRecvBuffer->setError(std::make_exception_ptr(std::system_error(mEC)));
							op.getCB(cbs, l);

							--mNumRecvs;
						}
						clearRecv = true;
					}
					else
					{
						auto src = std::move(mRecvCancelSrc);
						if (src.stop_possible())
							src.request_stop();
					}
				}

				if (clearSend)
					slot->mSendOps2_.clear();
				if (clearRecv)
					slot->mRecvOps2_.clear();

				cbs.get().resume();
			}
		}

		coroutine_handle<> SockScheduler::flush(coroutine_handle<> h)
		{
			Lock l(mMutex);
			if (mNumRecvs == 0 && mSendBuffers_.size() == 0)
				return h;

			auto f = std::make_shared<FlushToken>(h);

			for (auto& slot : mSlots_)
			{
				for (auto& recv : slot.mRecvOps2_)
				{
					recv.mFlushes.push_back(f);
				}
				for (auto& send : slot.mSendOps2_)
				{
					send.mFlushes.push_back(f);
				}
			}

			return macoro::noop_coroutine();
		}

		void SockScheduler::close(
			CBList& cbs,
			Caller c,
			bool& closeSocket,
			error_code ec,
			Lock&l)
		{

			if (mClosed == false)
			{
				mEC = ec;
				closeSocket = true;
				mClosed = true;
			}

			if (mSendStatus != Status::InUse || c == Caller::Sender)
			{
				mSendStatus = Status::Closed;
				while (mSendBuffers_.size())
				{
#ifdef COPROTO_SOCK_LOGGING
					mSendLog.push_back("close");
#endif
					auto& slot = *mSendBuffers_.front();
					auto& op = slot.mSendOps2_.front();

					op.mSendBuff.setError(std::make_exception_ptr(std::system_error(mEC)));
					op.getCB(cbs, l);

					slot.mSendOps2_.pop_front();
					mSendBuffers_.pop_front();
				}
			}
			else
			{
#ifdef COPROTO_SOCK_LOGGING
				mSendLog.push_back("try-close");
#endif
			}


			if (mRecvStatus != Status::InUse || c == Caller::Recver)
			{
#ifdef COPROTO_SOCK_LOGGING
				mRecvLog.push_back("close");
#endif
				mRecvStatus = Status::Closed;
				for (auto& slot : mSlots_)
				{
					for (auto& op : slot.mRecvOps2_)
					{
						op.mRecvBuffer->setError(std::make_exception_ptr(std::system_error(mEC)));
						op.getCB(cbs, l);

					}
					slot.mRecvOps2_.clear();
				}
				mNumRecvs = 0;
			}
			else
			{
#ifdef COPROTO_SOCK_LOGGING
				mRecvLog.push_back("try-close");
#endif
			}

		}

		void SockScheduler::close()
		{
			CBList cbs;
			bool closeSocket = false;
			{
				Lock l(mMutex);
				close(cbs, Caller::Extern, closeSocket, code::closed, l);
			}

			if (closeSocket)
				mCloseSock();

			cbs.get().resume();
		}

		SockScheduler::anyRecvOp::anyRecvOp(SockScheduler* ss, error_code e)
			: s(ss)
			, ec(e)
		{}

		bool SockScheduler::anyRecvOp::await_ready() { return false; }


#ifdef COPROTO_CPP20
		std::coroutine_handle<> SockScheduler::anyRecvOp::await_suspend(std::coroutine_handle<>h) {
			return await_suspend(coroutine_handle<>(h)).std_cast();
		}
#endif
		coroutine_handle<> SockScheduler::anyRecvOp::await_suspend(coroutine_handle<>h) {
			if (s->mInitializing)
			{
				s->mRecvStatus = SockScheduler::Status::Idle;
				COPROTO_ASSERT(!s->mRecvTaskHandle);
				s->mRecvTaskHandle = h;
				return macoro::noop_coroutine();
			}

			Slot* slot = nullptr;
			CBList cbs;
			if (s->mRequestedRecvSlot)
			{
				slot = &*std::exchange(s->mRequestedRecvSlot, nullopt).value();
				auto& op = slot->mRecvOps2_.front();

				if (ec)
					op.mRecvBuffer->setError(std::make_exception_ptr(std::system_error(ec)));
				if (ec == code::operation_aborted)
					ec = {};
				op.mReg.reset();
			}

			bool closeSocket = false;

			{
				auto lock = SockScheduler::Lock(s->mMutex);

				if (s->mRecvCancelSrc.stop_possible() == false)
					s->resetRecvToken();

				if (slot)
				{
#ifdef COPROTO_SOCK_LOGGING
					s->mRecvLog.push_back("anyRecvOp::pop-recv");
#endif
					slot->mRecvOps2_.front().getCB(cbs, lock);
					slot->mRecvOps2_.pop_front();

					COPROTO_ASSERT(s->mNumRecvs);
					--s->mNumRecvs;
				}

				if (s->mClosed || ec)
				{
#ifdef COPROTO_SOCK_LOGGING
					s->mRecvLog.push_back("anyRecvOp::closing");
#endif
					s->close(cbs, Caller::Recver, closeSocket, ec, lock);

					if (!cbs)
					{
						assert(cbs.mSize == 0);
						cbs.push_back({}, macoro::noop_coroutine());
					}
				}
				else
				{
					if (slot && slot->mClosed)
					{
						assert(0 && "not impl");
					}

					if (s->mNumRecvs)
					{
#ifdef COPROTO_SOCK_LOGGING
						s->mRecvLog.push_back("anyRecvOp::resume");
#endif
						cbs.push_back({}, h);
					}
					else
					{
#ifdef COPROTO_SOCK_LOGGING
						s->mRecvLog.push_back("anyRecvOp::idle");
#endif
						s->mRecvStatus = SockScheduler::Status::Idle;
						COPROTO_ASSERT(!s->mRecvTaskHandle);
						s->mRecvTaskHandle = h;

						if (!cbs)
						{
							assert(cbs.mSize == 0);
							cbs.push_back({}, macoro::noop_coroutine());
						}
					}
				}
			}

			if (closeSocket)
				s->mCloseSock();

			COPROTO_ASSERT(cbs);
			return cbs.get();
			//for (u64 i = 0; i < cbs.size() - 1; ++i)
			//	cbs[i].resume();
			//return cbs.back();
		}


		bool SockScheduler::getRequestedRecvSlot::await_ready() { return false; }
		coroutine_handle<> SockScheduler::getRequestedRecvSlot::await_suspend(coroutine_handle<>h)
		{
			auto lock = SockScheduler::Lock(s->mMutex);

			if (s->mClosed)
				return h;

			s->mRequestedRecvSlot = s->getRecvHeaderSlot(lock);
			if (!s->mRequestedRecvSlot)
				return h;

			auto& slot = (**s->mRequestedRecvSlot);
			if (!slot.mRecvOps2_.size())
			{
#ifdef COPROTO_SOCK_LOGGING
				s->mRecvLog.push_back("getRequestedRecvSlot::idle");
#endif
				s->mRecvStatus = SockScheduler::Status::RequestedRecvOp;
				COPROTO_ASSERT(!s->mRecvTaskHandle);
				s->mRecvTaskHandle = h;
				return macoro::noop_coroutine();
			}
			else
			{
#ifdef COPROTO_SOCK_LOGGING
				s->mRecvLog.push_back("getRequestedRecvSlot::resume");
#endif
				slot.mRecvOps2_.front().mInProgress = true;
				return h;
			}
		}
#ifdef COPROTO_CPP20
		std::coroutine_handle<> SockScheduler::getRequestedRecvSlot::await_suspend(std::coroutine_handle<> h)
		{
			return await_suspend(coroutine_handle<>(h)).std_cast();
		}
#endif


		optional<SockScheduler::SlotIter> SockScheduler::getRequestedRecvSlot::await_resume()
		{
			if (s->mRequestedRecvSlot)
			{
				auto& slot = **s->mRequestedRecvSlot;
				std::ignore = slot;
				assert(slot.mRecvOps2_.size() && slot.mRecvOps2_.front().mInProgress);
			}
			return s->mRequestedRecvSlot;
		}


		bool SockScheduler::NextSendOp::await_ready() { return false; }

		coroutine_handle<> SockScheduler::NextSendOp::await_suspend(coroutine_handle<> h)
		{
			if (s->mInitializing)
			{
				s->mSendStatus = Status::Idle;
				COPROTO_ASSERT(!s->mSendTaskHandle);
				s->mSendTaskHandle = h;
				return macoro::noop_coroutine();
			}

			bool closeSocket = false;
			auto slot = s->mSendBuffers_.front();
			auto& op = slot->mSendOps2_.front();
			CBList cbs;

			// could lock s->mMutex
			op.mReg.reset();
			if (ec)
			{
#ifdef COPROTO_SOCK_LOGGING
				s->mSendLog.push_back("error");
#endif
				op.mSendBuff.setError(std::make_exception_ptr(std::system_error(ec)));
			}
			if (ec == code::operation_aborted)
			{
				COPROTO_ASSERT(s->mSendCancelSrc.stop_possible() == false);
				ec = {};
			}

			{
				auto lock = Lock(s->mMutex);
				slot->mSendOps2_.front().getCB(cbs, lock);
				slot->mSendOps2_.pop_front();
				s->mSendBuffers_.pop_front();

				if (s->mSendCancelSrc.stop_possible() == false)
					s->resetSendToken();

				if (s->mClosed || ec)
				{
#ifdef COPROTO_SOCK_LOGGING
					s->mSendLog.push_back("error-close");
#endif
					s->close(cbs, Caller::Sender, closeSocket, ec, lock);
				}
				else if (s->mSendBuffers_.size())
				{
					if (slot->mClosed)
					{
						assert(0 && "not impl");
						//for (auto& op2 : slot->mSendOps2_)
						//{
						//	op2.mSendBuff.setError(std::make_exception_ptr(std::system_error(code::operation_aborted)));
						//	cbs.push_back(op2.mCH);
						//	COPROTO_ASSERT(0 && "delete mSendBuffers_" );
						//}
					}
					else
					{

#ifdef COPROTO_SOCK_LOGGING
						s->mSendLog.push_back("continue");
#endif
						auto& slot2 = *s->mSendBuffers_.front();
						auto& op2 = slot2.mSendOps2_.front();
						op2.mInProgress = true;
						cbs.push_back({}, h);
					}
				}
				else
				{
#ifdef COPROTO_SOCK_LOGGING
					s->mSendLog.push_back("idle");
#endif
					s->mSendStatus = Status::Idle;
					COPROTO_ASSERT(!s->mSendTaskHandle);
					s->mSendTaskHandle = h;
					if(!cbs)
						cbs.push_back({}, macoro::noop_coroutine());
				}
			}

			if (closeSocket)
				s->mCloseSock();

			COPROTO_ASSERT(cbs);
			return cbs.get();
		}

#ifdef COPROTO_CPP20
		std::coroutine_handle<> SockScheduler::NextSendOp::await_suspend(std::coroutine_handle<> h) {
			return await_suspend(coroutine_handle<>(h)).std_cast();
		}
#endif


		SockScheduler::SlotIter SockScheduler::NextSendOp::await_resume() {

			assert(s->mSendBuffers_.size());

			auto iter = s->mSendBuffers_.front();
			assert((*iter).mSendOps2_.front().mInProgress);
			return iter;
		}
	}


}
int debugCounter = 0;
