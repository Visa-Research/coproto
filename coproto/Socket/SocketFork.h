#pragma once
#include "coproto/Common/Defines.h"
#include "coproto/Common/Queue.h"
#include "coproto/Proto/SessionID.h"
#include "coproto/Socket/RecvOperation.h"
#include "coproto/Socket/SendOperation.h"

namespace coproto::internal
{
	struct SockScheduler;



	// the state associated with a fork of the socket.
	// each fork will have a session id, a 128 unique ID.
	// This is then mapped to a "local" id and "remote" id.
	// these are shorter names for the same thing. The local 
	// party uses the local id and the remote party uses 
	// remote id.
	//
	// a fork can also refer to a executor. If set, all 
	// completions will be enqued there.
	struct SocketFork
	{
		SocketFork(SessionID id)
			: mSessionID(id)
		{}

		// the sesssion id of the fork.
		SessionID mSessionID;

		// the local and remote is that is used when sending data.
		u32 mLocalId = -1, mRemoteId = -1;

		//u64 mRecvIdx = 0;

		// a flag indicating that this fork has send the local id
		// to the remote party. At the start, the remote party does
		// not have the mSessionID -> mRemoteID mapping. So we will
		// send it as the first message. After that, we can just 
		// send messages with our smaller local id.
		bool mInitiated = false;

		// an optional executor.
		ExecutorRef mExecutor;

		// a name that can be set for debugging. Not typically used.
		std::string mName;

	private:
		// the queue of recv operations assoicated with this fork.
		Queue<RecvOperation> mRecvOps;

		// the queue of send operations assoicated with this fork.
		Queue<SendOperation> mSendOps;
	public:

		template<typename... Args>
		SendOperation& emplace_send(Lock&, Args&&... args)
		{
			mSendOps.emplace_back(std::forward<Args>(args)...);
			return mSendOps.back();
		}

		template<typename... Args>
		RecvOperation& emplace_recv(Lock&, Args&&... args)
		{
			mRecvOps.emplace_back(std::forward<Args>(args)...);
			return mRecvOps.back();
		}

		template<typename... Args>
		void pop_front_send(Lock&)
		{
			mSendOps.pop_front();
		}

		template<typename... Args>
		void pop_front_recv(Lock&)
		{
			mRecvOps.pop_front();
		}

		void erase_send(Lock&, SendOperation* ptr)
		{
			mSendOps.erase(ptr);
		}

		void erase_recv(Lock&, RecvOperation* ptr)
		{
			mRecvOps.erase(ptr);
		}

		auto size_send(Lock&)
		{
			return mSendOps.size();
		}
		auto size_recv(Lock&)
		{
			return mRecvOps.size();
		}

		auto& front_send(Lock&)
		{
			return mSendOps.front();
		}
		auto& front_recv(Lock&)
		{
			return mRecvOps.front();
		}
		auto& back_send(Lock&)
		{
			return mSendOps.back();
		}
		auto& back_recv(Lock&)
		{
			return mRecvOps.back();
		}
	};
	
	//inline
	//	u64& recvIndex(SocketFork* s)
	//{
	//	return s->mRecvIdx;
	//}

	inline void SendOperation::completeOn(ExecutionQueue::Handle& queue, Lock& l)
	{
		assert(mCH);
		queue.push_back(std::exchange(mCH, nullptr), mSocketFork->mExecutor, l);
		for (auto& f : mFlushes)
		{
			if (f.use_count() == 1)
			{
				queue.push_back(std::exchange(f->mHandle, nullptr), mSocketFork->mExecutor, l);
			}
		}
		mFlushes.clear();
	}

	inline void RecvOperation::completeOn(ExecutionQueue::Handle& queue, Lock& l)
	{
		assert(mCH);
		queue.push_back(std::exchange(mCH, nullptr), mSocketFork->mExecutor, l);
		for (auto& f : mFlushes)
		{
			if (f.use_count() == 1)
			{
				queue.push_back(std::exchange(f->mHandle, nullptr), mSocketFork->mExecutor, l);
			}
		}
		mFlushes.clear();
	}

	//using SocketForkIter = std::list<SocketFork>::iterator;

}