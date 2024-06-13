#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "coproto/Common/Defines.h"
#include "coproto/Common/TypeTraits.h"
#include "coproto/Common/span.h"
#include "coproto/Common/InlinePoly.h"
#include "coproto/Common/error_code.h"
#include "coproto/Common/Function.h"
#include "coproto/Common/Queue.h"

#include "coproto/Proto/SessionID.h"
#include "coproto/Proto/Operation.h"

#include "coproto/Common/macoro.h"
#include "macoro/stop.h"

#include <mutex>
#include <atomic>
#include "coproto/Socket/Executor.h"
#include "coproto/Socket/SendOperation.h"
#include "coproto/Socket/RecvOperation.h"
#include "coproto/Socket/SocketFork.h"
#include "macoro/result.h"
#include "coproto/Common/Exceptions.h"
#include <cstring>

#ifdef COPROTO_SOCK_LOGGING
#define RECV_LOG(X) if(mLogging) mRecvLog.push_back(X)
#define SEND_LOG(X) if(mLogging) mSendLog.push_back(X)
#else
#define RECV_LOG(X)
#define SEND_LOG(X)
#endif

namespace coproto
{

	namespace internal
	{

		struct SockScheduler;


		// the message header encoding the size and fork id.
		struct Header
		{
			// the size in bytes of the message to be send next.
			u32 mSize;

			// the fork id of the sending party.
			u32 mForkId;
		};

		// a struct meant to encode various meta data.
		// right now the only thing its is used for is
		// sending the SessionId -> remoteID mapping.
		struct ControlBlock
		{
			// the data to be sent.
			std::array<u8, 16> data;
			enum class Type : u8
			{
				NewSocketFork = 1
			};

			Type getType() { return Type::NewSocketFork; }
			SessionID getSessionID() {
				SessionID ret;
				std::memcpy(ret.mVal, data.data(), 16);
				return ret;
			}

			void setType(Type t) {};
			void setSessionID(const SessionID& id) {
				std::memcpy(data.data(), id.mVal, 16);
			}
		};


		// an awaiter used to get tne next message to be sent.
		struct NextSendOp
		{
			NextSendOp(
				SendOperation* prevOp,
				error_code prevEc,
				SockScheduler& ss)
				: mPrevOp(prevOp)
				, mPrevEc(prevEc)
				, mSched(ss) {}

		private:
			SendOperation* mPrevOp;
			error_code mPrevEc;
			SockScheduler& mSched;
			std::coroutine_handle<> mHandle;
			macoro::result<SendOperation*, macoro::error_code> mRes;

		public:

			std::coroutine_handle<> getHandle(
				macoro::result<SendOperation*, macoro::error_code> r,
				NextSendOp*& self);

			void completePrev(Lock& lock, ExecutionQueue::Handle& queue);

			bool await_ready();

			std::coroutine_handle<> await_suspend(std::coroutine_handle<> h);

			macoro::result<SendOperation*, macoro::error_code> await_resume();
		};


		struct GetRequestedRecvSocketFork
		{
			GetRequestedRecvSocketFork(SockScheduler& ss, u32 remoteForkId)
				: mSched(ss)
				, mRemoteForkId(remoteForkId)
			{}
		private:
			macoro::result<RecvOperation*, std::error_code> mRes;
			SockScheduler& mSched;
			u32 mRemoteForkId;
			std::coroutine_handle<> mHandle;

		public:

			u32 forkID()
			{
				return mRemoteForkId;
			}

			std::coroutine_handle<> getHandle(
				macoro::result<RecvOperation*, std::error_code> r,
				GetRequestedRecvSocketFork*& self);

			bool await_ready() noexcept;

			std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept;

			macoro::result<RecvOperation*, std::error_code> await_resume();
		};


		// an awaiter used to wait until we have a requested
		// receive operation.
		struct AnyRecvOp
		{
			AnyRecvOp(
				RecvOperation* prevOp,
				error_code prevEc,
				SockScheduler& ss)
				: mPrevOp(prevOp)
				, mPrevEc(prevEc)
				, mSched(ss) {}

		private:
			RecvOperation* mPrevOp;
			error_code mPrevEc;
			std::optional<error_code> mRes;
			SockScheduler& mSched;
			std::coroutine_handle<> mHandle;


		public:

			std::coroutine_handle<> getHandle(error_code r, AnyRecvOp*& self);

			bool await_ready();

			void completePrev(Lock& lock, ExecutionQueue::Handle& queue);

			std::coroutine_handle<> await_suspend(std::coroutine_handle<>h);
			error_code await_resume();
		};


		struct CloseAwaiter
		{
			struct CloseAwaiterBase
			{
				virtual ~CloseAwaiterBase() {}
				bool await_ready() { return false; }
				virtual std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) = 0;
				virtual void await_resume() = 0;
			};

			template<typename Socket> struct CloseAwaiterImpl : CloseAwaiterBase
			{
				~CloseAwaiterImpl() {}

				CloseAwaiterImpl(SockScheduler* sched, Socket* sock)
					: mSched(sched)
					, mSock(sock) {}

				SockScheduler* mSched;
				Socket* mSock;
				using close_res = std::invoke_result_t<decltype(&Socket::close), Socket>;
				using Awaiter = std::conditional_t<std::is_void_v<close_res>, int , macoro::remove_rvalue_reference_t<close_res>>;
				std::optional<Awaiter> mAwaiter;


				std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) override;

				void await_resume() override;
			};

			~CloseAwaiter()
			{
				if (mPtr)
				{
					std::cout << "coproto::Socket::close() must be awaited if called. Terminate is being called" << std::endl;
					std::terminate();
				}
			}

			CloseAwaiterBase* mPtr;

			bool await_ready() { return mPtr->await_ready(); }
			std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) { return mPtr->await_suspend(h); }
			void await_resume() { return std::exchange(mPtr, nullptr)->await_resume(); }
		};


		// This class handles the logic with managing what message to
		// send and what fork/slot it corresponds to. Recall that we support running
		// multiple (semi) independent protocols on a single underlaying socket.
		// This is achieved by a small header to each message sent that can be used to
		// figure out who should receive any given message. This header will consist of
		// the size of the message and an id indicating what protocol this message 
		// corresponds to.
		// 
		// There are two types of messages:
		// 
		// Data - these messages have the format  [msg-size:32, slot-id:32, msg].
		//   * msg-size is a non-zero 32 bit value denoting the length in bytes of the message.
		//   * slot-id is a 32 bit value that is used to identify which slot this message corresponds to.
		//     This id should have previously been initialized, see below.
		//   * msg is the actual data of the message. This will consist of msg-size bytes.
		// 
		// Meta - these messages have the format [ zero:32, slot-id:32, meta-data]. 
		//   These messages are used to initialize new slots and other socket internal state.
		//   * zero is always value 0 and 32 bits long. This allows meta message to be 
		//     distinguished from data messages, which always start with a non-zero message.
		//   * slot-id is a 32 bit value to identify the slot-id that this meta message corresponds to.
		//   * meta-data is the data associated with this meta message. Currently there is only
		//     one option for this. In particular, the only meta message currently supported is 
		//     to create a new slot. This is done by sending a new/unused value for slot-id and the have
		//     meta-data be the 128-bit session ID corresponding to this slot/fork. Note that each party
		//     may associate a different slot-id with the same session ID. 
		// 
		//     Each fork/slot is associated with a unique/random-ish session ID. Instead of sending the 128 bit
		//     session ID with each message, we associate the session ID with a 32 bit slot-id.
		//     It is then sufficient. to only send the slot-id
		//   
		//
		// Whenever the first message is sent on a slot/fork, we must first initialize
		// the slot by sending a meta message as described above.
		// 
		// When a party requests to receive a message on a given fork/slot, we check 
		// if we have previously read the header corresponding to this receive. This
		// might happen if multiple slots have requested a receive. If we haven't
		// received the header, then first we to receive the header. If the received 
		// header is not for this slot then we will just suspend the receive task
		// until the user asks for the receive on that slot.
		// 
		// Another complicating detail is that we allow the user to move a buffer when 
		// sending it. In this case the async send operation will appear to complete
		// synchronously to the user. However, in reality it has just been buffered by
		// SockScheduler. As such, this can lead to the situation that the user may think
		// everything is done and destroy the socket. In this case we run into issue due to 
		// an async send still pending. As a workaround, we allow the user to "flush" the 
		// socket which will suspend the user until all messages have been sent.
		// 
		// Another complications is that the user can cancel send/recv operations.
		// In the event that one message is canceled, the user is still allowed to
		// send and receive messages on the socket. However, in the event that a message
		// is half sent we are forced to send the whole message because the receiver is 
		// expecting the whole message. One could partially fix this by breaking messages up  
		// into small chunks but the implementation does not do this. Instead, if the user
		// requests a cancel on a send, the operation is immediately canceled (assuming the 
		// underlaying socket cooperates). Later, if the message was half sent and the user
		// want to send some other message, the first message must be completed first.
		// 
		struct SockScheduler
		{

			// the status of the send and receive sockets.
			enum class Status
			{
				Idle,
				InUse,
				RequestedRecvOp,
				Closed
			};

			enum class Caller { Sender, Recver, Extern };

			NextSendOp* mNextSendOp = nullptr;
			NextSendOp completeOpAndGetNextSend(SendOperation* op, error_code ec)
			{
				return { op, ec, *this };
			}

			AnyRecvOp* mAnyRecvOp = nullptr;
			auto completeOpAndWaitForAnyRecv(RecvOperation* prevOp, error_code prevEc) {
				return AnyRecvOp{ prevOp, prevEc, *this };
			}


			GetRequestedRecvSocketFork* mGetRequestedRecvSocketFork = nullptr;
			auto getRequestedRecvSocketFork(u32 forkId)
			{
				return GetRequestedRecvSocketFork(*this, forkId);
			}

			// a flag indicating if close() has been awaited.
			bool mClosed = false;

			// a awaiter used to call socket.close().
			std::unique_ptr<CloseAwaiter::CloseAwaiterBase> mCloseSock;

			// storage used to store the socket.
			AnyNoCopy mSockStorage;

			// The list of forks.
			std::list<SocketFork> mSocketForks_;

			// the index of the next local fork id.
			u32 mNextLocalSocketFork = 1;

			// maps SessionID to slot index.
			std::unordered_map<SessionID, SocketForkIter> mIdSocketForkMapping_;

			// maps their slot index to SessionID
			std::unordered_map<u32, SocketForkIter> mRemoteSocketForkMapping_;

			// a mutex used to guard member variables.
			std::recursive_mutex mMutex;

			// an intrusive linked list of send buffers.
			SendOperation* mSendBufferBegin = nullptr;
			SendOperation* mSendBufferLast = nullptr;

			// the current overall error code.
			error_code mEC;

			// the current status of the recv coroutine.
			Status mRecvStatus = Status::Idle;

			// the current status of the send coroutine.
			Status mSendStatus = Status::Idle;

			// metrics, the total number of receive operations
			u64 mNumRecvs = 0;
			// metrics, the total number bytes sent and received.
			u64 mBytesSent = 0, mBytesReceived = 0;

			macoro::blocking_task<macoro::task<>> mSendTask;
			macoro::blocking_task<macoro::task<>> mRecvTask;

			ExecutionQueue mExQueue;


			void resetRecvToken()
			{
				assert(mRecvCancelSrc.stop_possible() == false);

				mRecvCancelSrc = macoro::stop_source();
				mRecvToken = mRecvCancelSrc.get_token();
			}
			void resetSendToken()
			{
				assert(mSendCancelSrc.stop_possible() == false);
				mSendCancelSrc = macoro::stop_source();
				mSendToken = mSendCancelSrc.get_token();
			}

			macoro::stop_token mRecvToken, mSendToken;
			macoro::stop_source mRecvCancelSrc, mSendCancelSrc;

			void* mSockPtr = nullptr;

			void* getSocket()
			{
				return mSockPtr;
			}

			template<typename SocketImpl>
			void init(SocketImpl* sock, SessionID sid);

			template<typename SocketImpl>
			SockScheduler(SocketImpl&& s, SessionID sid);

			template<typename SocketImpl>
			SockScheduler(SocketImpl& s, SessionID sid);

			template<typename SocketImpl>
			SockScheduler(std::unique_ptr<SocketImpl>&& s, SessionID sid);


			~SockScheduler()
			{
				if (mRecvStatus == Status::InUse || mSendStatus == Status::InUse)
				{
					std::cout << "Socket was destroyed with pending operations. "
						<< "terminate() is being called. Await Socket::flush() "
						<< "before the destructor is called. This will ensure that"
						<< "all pending operations complete. " << MACORO_LOCATION << std::endl;
					std::terminate();
				}
				close();
				mSendTask.get();
				mRecvTask.get();
			}

			template<typename Sock>
			macoro::task<> makeSendTask(Sock* socket);

			std::vector<const char*> mRecvLog, mSendLog;

			template<typename Sock>
			macoro::task<> receiveDataTask(Sock* socket);

			SocketForkIter getLocalSocketFork(const SessionID& id, Lock& _);

			void initLocalSocketFork(const SessionID& id, const ExecutorRef& ex, Lock& _);
			error_code initRemoteSocketFork(u32 slotId, SessionID id, Lock& _);

			SessionID fork(SessionID s);

			template<typename Buffer>
			MACORO_NODISCARD coroutine_handle<void> send(
				SessionID id,
				Buffer&& buffer,
				coroutine_handle<void> callback,
				macoro::stop_token&& token);

			MACORO_NODISCARD
				coroutine_handle<void> recv(SessionID id, RecvBuffer* data, coroutine_handle<void> ch, macoro::stop_token&& token);


			void cancel(
				ExecutionQueue::Handle& queue,
				Caller c,
				error_code,
				Lock&);

			void close();

			coroutine_handle<> flush(coroutine_handle<>h);

			bool mLogging = false;
			void enableLogging()
			{
				mRecvLog.reserve(1000);
				mSendLog.reserve(1000);
				mLogging = true;
			}

			void disableLogging()
			{
				mLogging = false;
			}

			template<typename Scheduler>
			void setExecutor(Scheduler& scheduler, SessionID id)
			{
				Lock lock(mMutex);
				auto iter = getLocalSocketFork(id, lock);
				iter->mExecutor = ExecutorRef(scheduler);
			}
		};


		template<typename SocketImpl>
		void SockScheduler::init(SocketImpl* sock, SessionID sid)
		{
			mSockPtr = sock;
			mExQueue.setMutex(mMutex);
			//auto mCloseSock = [this, sock](std::coroutine_handle<> h) {

			//	auto awaiter = sock->close();

			//	};


			mRecvToken = mRecvCancelSrc.get_token();
			mSendToken = mSendCancelSrc.get_token();
			Lock l;
			initLocalSocketFork(sid, {}, l);


			//mCloseSock = [this, sock] 
			mCloseSock = std::unique_ptr<CloseAwaiter::CloseAwaiterBase>(new CloseAwaiter::CloseAwaiterImpl<SocketImpl>{ this, sock });

			mRecvTask = macoro::make_blocking(receiveDataTask(sock));
			mSendTask = macoro::make_blocking(makeSendTask(sock));
		}

		template<typename SocketImpl>
		SockScheduler::SockScheduler(SocketImpl&& s, SessionID sid)
		{
			auto ss = mSockStorage.emplace(std::move(s));
			init(ss, sid);
		}

		template<typename SocketImpl>
		SockScheduler::SockScheduler(SocketImpl& s, SessionID sid)
		{
			init(&s, sid);
		}

		template<typename SocketImpl>
		SockScheduler::SockScheduler(std::unique_ptr<SocketImpl>&& s, SessionID sid)
		{
			auto ss = mSockStorage.emplace(std::move(s));
			init(ss->get(), sid);
		}



		template<typename Buffer>
		MACORO_NODISCARD coroutine_handle<void> SockScheduler::send(
			SessionID id,
			Buffer&& buffer,
			coroutine_handle<void> callback,
			macoro::stop_token&& token)
		{
			assert(callback);
			if (buffer.asSpan().size() == 0)
			{
				buffer.setError(code::sendLengthZeroMsg);
				return callback;
			}

			ExecutionQueue::Handle exQueue;

			{
				Lock l = Lock(mMutex);
				exQueue = mExQueue.acquire(l);
				auto fork = getLocalSocketFork(id, l);

				if (mEC)
				{
					buffer.setError(mEC);
					exQueue.push_back(callback, fork->mExecutor, l);
				}
				else
				{
					auto opPtr = &fork->emplace_send(l,
						fork, callback, std::move(buffer));

					if (mNextSendOp)
					{
						COPROTO_ASSERT(mSendStatus == Status::Idle);
						mSendStatus = Status::InUse;
						opPtr->setStatus(SendOperation::Status::InProgress);

						assert(mSendBufferBegin == nullptr);
						assert(mSendBufferLast == nullptr);

						exQueue.push_back(mNextSendOp->getHandle(macoro::Ok(opPtr), mNextSendOp), {}, l);

						mSendBufferBegin = opPtr;
						mSendBufferLast = opPtr;
					}
					else
					{
						assert(mNextSendOp == nullptr);
						assert(mSendBufferLast != nullptr);

						mSendBufferLast->setNext(opPtr);
						mSendBufferLast = opPtr;
					}

					opPtr->setCancelation(std::move(token), [this, opPtr] {
						macoro::stop_source cancelSrc;
						ExecutionQueue::Handle exQueue;
						{
							Lock l(mMutex);
							exQueue = mExQueue.acquire(l);
							if (opPtr->status() == SendOperation::Status::NotStarted)
							{
								// we will skip this operation and calls its cb
								opPtr->setError(code::operation_aborted);
								opPtr->completeOn(exQueue, l);
								assert(opPtr->prev());
								opPtr->prev()->setNext(opPtr->next());
								opPtr->fork().erase_send(l, opPtr);
							}
							else
							{
								opPtr->setStatus(SendOperation::Status::Canceling);
								// the operation is in progress, call cancel.
								// in this case we must have alrady released then enque 
								// lock and we are only holding the current lock.
								if (cancelSrc.stop_possible())
									exQueue.push_back_fn([cancelSrc = std::move(mSendCancelSrc)]() mutable {
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

























		inline std::coroutine_handle<> GetRequestedRecvSocketFork::getHandle(
			macoro::result<RecvOperation*, std::error_code> r,
			GetRequestedRecvSocketFork*& self)
		{
			COPROTO_ASSERT(this == self);
			COPROTO_ASSERT(r.has_error() || r.value());
			self = nullptr;
			mRes = std::move(r);
			return std::exchange(mHandle, nullptr);
		}

		inline bool GetRequestedRecvSocketFork::await_ready() noexcept { return false; }

		inline std::coroutine_handle<> GetRequestedRecvSocketFork::await_suspend(std::coroutine_handle<> h) noexcept
		{
			auto& mLogging = mSched.mLogging;
			auto& mRecvLog = mSched.mRecvLog;
			(void)mLogging;
			(void)mRecvLog;

			ExecutionQueue::Handle queue;
			{

				auto lock = Lock(mSched.mMutex);
				queue = mSched.mExQueue.acquire(lock);

				// make sure the fork ID they sent exist.
				auto iter = mSched.mRemoteSocketForkMapping_.find(mRemoteForkId);
				if (iter == mSched.mRemoteSocketForkMapping_.end())
				{
					mSched.cancel(queue, SockScheduler::Caller::Recver, code::badCoprotoMessageHeader, lock);
				}

				if (mSched.mEC)
				{
					mRes = macoro::Err(mSched.mEC);
					queue.push_back(h, {}, lock);
				}
				else
				{

					// get the fork and set the return value.
					auto& fork = *iter->second;

					// check of we have a matching recv
					if (fork.size_recv(lock) == 0)
					{
						// ok, data has arrived but we dont have anywhere 
						// to store it. We will store the continuation
						// in the scheduler. Once the matching recv request
						// arrives, we will resume.
						RECV_LOG("getRequestedRecvSocketFork::idle");
						mSched.mRecvStatus = SockScheduler::Status::RequestedRecvOp;
						mHandle = h;
						mSched.mGetRequestedRecvSocketFork = this;
					}
					else
					{
						RECV_LOG("getRequestedRecvSocketFork::resume");

						mRes = macoro::Ok(&fork.front_recv(lock));
						fork.front_recv(lock).setStatus(RecvOperation::Status::InProgress);
						queue.push_back(h, {}, lock);
					}
				}
			}
			return queue.runReturnLast().std_cast();
		}

		inline macoro::result<RecvOperation*, std::error_code> GetRequestedRecvSocketFork::await_resume()
		{
			assert(mRes.has_error() || mRes.value());
			return std::move(mRes);
		}


		inline std::coroutine_handle<> AnyRecvOp::getHandle(error_code r, AnyRecvOp*& self)
		{
			COPROTO_ASSERT(this == self);
			self = nullptr;
			mRes = std::move(r);
			return std::exchange(mHandle, nullptr);
		}

		inline bool AnyRecvOp::await_ready() { return false; }

		inline void AnyRecvOp::completePrev(Lock& lock, ExecutionQueue::Handle& queue)
		{
			COPROTO_ASSERT(mPrevOp);
			auto& op = *mPrevOp;

			if (mSched.mRecvCancelSrc.stop_possible() == false)
				mSched.resetRecvToken();

			//RECV_LOG("anyRecvOp::pop-recv");
			auto& fork = op.fork();
			//std::cout << "pop_front_recv " << op.mIndex << " f " << fork.mLocalId << " " << (size_t)&mSched << std::endl;
			COPROTO_ASSERT(
				mSched.mNumRecvs &&
				fork.size_recv(lock) &&
				&fork.front_recv(lock) == &op);

			if (mPrevEc)
			{
				op.setError(std::exchange(mPrevEc, code::cancel));
			}
			op.completeOn(queue, lock);
			fork.pop_front_recv(lock);
			--mSched.mNumRecvs;
		}

		inline std::coroutine_handle<> AnyRecvOp::await_suspend(std::coroutine_handle<>h)
		{
			ExecutionQueue::Handle queue;
			{
				auto lock = Lock(mSched.mMutex);
				queue = mSched.mExQueue.acquire(lock);

				if (mPrevOp)
					completePrev(lock, queue);

				if (mPrevEc || mSched.mEC)
				{
					mRes.emplace(code::cancel);
					mSched.cancel(queue, SockScheduler::Caller::Recver, mPrevEc, lock);
					queue.push_back(h, {}, lock);
				}
				else
				{
					if (mSched.mNumRecvs)
					{
						mRes.emplace(code::success);
						queue.push_back(h, {}, lock);
					}
					else
					{
						auto& mLogging = mSched.mLogging;
						auto& mRecvLog = mSched.mRecvLog;
						(void)mLogging;
						(void)mRecvLog;

						RECV_LOG("anyRecvOp::idle");
						mSched.mRecvStatus = SockScheduler::Status::Idle;
						mSched.mAnyRecvOp = this;
						mHandle = h;
					}
				}
			}

			return queue.runReturnLast().std_cast();
		}

		inline error_code AnyRecvOp::await_resume() {
			COPROTO_ASSERT(mRes.has_value());
			return mRes.value();
		}



		template<typename Sock>
		macoro::task<void> SockScheduler::receiveDataTask(Sock* sock)
		{
			auto checkRecv = [](error_code ec, u64 bytesTransmitted, u64 bufferSize) {
				if (!ec && bytesTransmitted != bufferSize)
					ec = code::ioError;
				return ec;
				};

			RecvOperation* op = nullptr;
			error_code ec = code::success;
			u64 bt;
			while (true)
			{
			Next:
				// await until we have at least one receive operation
				if (co_await completeOpAndWaitForAnyRecv(
					std::exchange(op, nullptr),
					std::exchange(ec, {})))
					break;

				RECV_LOG("new-recv");

				Header header;

				// the first thing we need to do is get a receive header.
				// This will let us know what receive operation to handle
				// next. We do this in a while loop because we might receive 
				// more than one header if a header contains only meta data.
				while (true)
				{
					// recev the header
					RECV_LOG("recving-header");
					std::tie(ec, bt) = co_await sock->recv(asSpan(header), mRecvToken);
					mBytesReceived += bt;
					if (checkRecv(ec, bt, sizeof(header)))
						goto Next;

					// the message size will be zero if its meta-data
					if (header.mSize == 0)
					{
						RECV_LOG("recving-header-meta");

						ControlBlock metadata;
						std::tie(ec, bt) = co_await sock->recv(asSpan(metadata), mRecvToken);
						mBytesReceived += bt;
						if (checkRecv(ec, bt, sizeof(metadata)))
							goto Next;

						auto slotId = header.mForkId;
						auto sid = metadata.getSessionID();
						auto lock = Lock(mMutex);
						ec = initRemoteSocketFork(slotId, sid, lock);
						if (ec)
							goto Next;
					}
					else
					{
						break;
					}
				}

				RECV_LOG("getRequestedRecvSocketFork-enter");
				auto opRes = co_await getRequestedRecvSocketFork(header.mForkId);
				if (opRes.has_error())
				{
					ec = opRes.error();
					goto Next;
				}

				op = opRes.value();
				span<u8> buffer = op->asSpan(header.mSize);

				if (buffer.size() != header.mSize)
				{
					ec = code::badBufferSize;
					goto Next;
				}


				RECV_LOG("recving-body");
				std::tie(ec, bt) = co_await sock->recv(buffer, mRecvToken);
				mBytesReceived += bt;

				if (checkRecv(ec, bt, buffer.size()))
					goto Next;

				RECV_LOG("recv-done");

			}

		}















		inline std::coroutine_handle<> NextSendOp::getHandle(
			macoro::result<SendOperation*, macoro::error_code> r,
			NextSendOp*& self)
		{
			COPROTO_ASSERT(this == self);
			self = nullptr;
			COPROTO_ASSERT(r.has_error() || r.value());
			mRes = std::move(r);
			return std::exchange(mHandle, nullptr);
		}


		inline void NextSendOp::completePrev(Lock& lock, ExecutionQueue::Handle& queue)
		{
			COPROTO_ASSERT(mPrevOp);
			auto& op = *mPrevOp;
			if (mPrevEc)
			{
				op.setError(std::exchange(mPrevEc, code::cancel));
			}
			op.completeOn(queue, lock);
			auto next = op.next();
			op.setNext(nullptr);

			assert(&op.fork().front_send(lock) == &op);
			assert(mSched.mSendBufferBegin == &op);

			mSched.mSendBufferBegin = next;
			if (next == nullptr)
				mSched.mSendBufferLast = nullptr;

			op.fork().pop_front_send(lock);
		}

		inline bool NextSendOp::await_ready() { return false; }
		inline std::coroutine_handle<> NextSendOp::await_suspend(std::coroutine_handle<> h)
		{
			ExecutionQueue::Handle queue;
			{
				auto lock = Lock(mSched.mMutex);
				queue = mSched.mExQueue.acquire(lock);
				if (mPrevOp)
					completePrev(lock, queue);
				if (mPrevEc || mSched.mEC)
				{
					COPROTO_ASSERT(mSched.mSendBufferBegin == nullptr || mSched.mSendBufferBegin->status() == SendOperation::Status::NotStarted);
					mSched.cancel(queue, SockScheduler::Caller::Sender, mPrevEc, lock);
					mRes = macoro::Err(code::closed);
					queue.push_back(h, {}, lock);
				}
				else
				{
					if (mSched.mSendBufferBegin)
					{
						COPROTO_ASSERT(mSched.mSendBufferBegin->status() == SendOperation::Status::NotStarted);
						mSched.mSendBufferBegin->setStatus(SendOperation::Status::InProgress);
						mRes = macoro::Ok(mSched.mSendBufferBegin);
						queue.push_back(h, {}, lock);
					}
					else
					{
						mSched.mSendStatus = SockScheduler::Status::Idle;
						mSched.mNextSendOp = this;
						mHandle = h;
					}
				}
			}

			return queue.runReturnLast().std_cast();
		}

		inline macoro::result<SendOperation*, macoro::error_code> NextSendOp::await_resume()
		{
			COPROTO_ASSERT(mRes.has_error() || mRes.value());
			return mRes;
		}



		template<typename Sock>
		macoro::task<void> SockScheduler::makeSendTask(Sock* sock)
		{
			auto checkSend = [](error_code ec, u64 bytesTransmitted, u64 bufferSize) {
				if (!ec && bytesTransmitted != bufferSize)
					ec = code::ioError;
				return ec;
				};

			SendOperation* op = nullptr;
			error_code ec;
			u64 bt;
			while (true)
			{
				auto opRes = co_await completeOpAndGetNextSend(
					std::exchange(op, nullptr),
					std::exchange(ec, {}));

				if (opRes.has_error())
					break;
				SEND_LOG("new-send");

				op = opRes.value();
				auto& fork = op->fork();
				auto data = op->asSpan();

				COPROTO_ASSERT(op->status() != SendOperation::Status::NotStarted);
				COPROTO_ASSERT(data.size() != 0);
				COPROTO_ASSERT(data.size() < std::numeric_limits<u32>::max());
				COPROTO_ASSERT(fork.mLocalId != ~u32(0));

				if (fork.mInitiated == false)
				{
					SEND_LOG("meta");

					struct SendControlBlock
					{
						Header mHeader;
						ControlBlock mCtrlBlk;
					};

					fork.mInitiated = true;
					SendControlBlock meta;
					meta.mHeader.mSize = 0;
					meta.mHeader.mForkId = fork.mLocalId;
					meta.mCtrlBlk.setType(ControlBlock::Type::NewSocketFork);
					meta.mCtrlBlk.setSessionID(fork.mSessionID);

					std::tie(ec, bt) = co_await sock->send(asSpan(meta), mSendToken);
					SEND_LOG("meta-sent");

					mBytesSent += bt;
					if (checkSend(ec, bt, sizeof(meta)))
						continue;
				}

				Header header;
				header.mForkId = fork.mLocalId;
				header.mSize = static_cast<u32>(data.size());
				SEND_LOG("sending-header");

				std::tie(ec, bt) = co_await sock->send(asSpan(header), mSendToken);
				mBytesSent += bt;
				if (checkSend(ec, bt, sizeof(header)))
					continue;

				SEND_LOG("sending-body");
				std::tie(ec, bt) = co_await sock->send(data, mSendToken);
				mBytesSent += bt;

				if (checkSend(ec, bt, data.size()))
					continue;

				SEND_LOG("send-done");
			}

			SEND_LOG("exit");
		}




		template<typename Socket>
		std::coroutine_handle<> CloseAwaiter::CloseAwaiterImpl<Socket>::await_suspend(std::coroutine_handle<> h) 
		{
			mSched->mClosed = true;
			if constexpr (std::is_void_v<close_res>)
			{
				mSock->close();
				return h;
			}
			else
			{
				mAwaiter.emplace(mSock->close());

				if (mAwaiter->await_ready())
					return h;

				using await_suspend_res = std::invoke_result_t<decltype(&Awaiter::await_suspend), Awaiter, std::coroutine_handle<>>;
				if constexpr (std::is_void_v<await_suspend_res >)
				{
					mAwaiter->await_suspend(h);
					return std::noop_coroutine();
				}
				else
					return mAwaiter->await_suspend(h);
			}
		};

		template<typename Socket>
		void CloseAwaiter::CloseAwaiterImpl<Socket>::await_resume()
		{

			if constexpr (std::is_void_v<close_res> == false)
				mAwaiter->await_resume();
		}
	}
}

#undef RECV_LOG
#undef SEND_LOG
