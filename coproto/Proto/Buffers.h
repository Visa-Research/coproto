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
#include "coproto/Common/macoro.h"


#include "coproto/Socket/SocketScheduler.h"
#include <source_location>
#include "macoro/trace.h"

namespace coproto
{
	//struct SockScheduler;



	inline void addTraceRethrow(std::exception_ptr ptr, std::vector<std::source_location>& stack)
	{
		auto stackString = [&]() ->std::string 
		{
			try {
				std::stringstream ss;
				ss << "\n";
				for (auto i = 0ull; i < stack.size(); ++i)
				{
					ss << i << " " << stack[i].file_name() << ":" << stack[i].line() << "\n";
				}
				return ss.str();
			}
			catch (...)
			{
			}
			return "";
		};

		try {
			std::rethrow_exception(ptr);
		}
		catch (BadReceiveBufferSize& e)
		{
			throw BadReceiveBufferSize(e.mBufferSize, e.mReceivedSize, stackString());
		}
		catch (std::system_error& e)
		{
			throw std::system_error(e.code(), stackString());
		}
		catch (std::exception& e)
		{
			throw std::runtime_error(e.what() + stackString());
		}
		catch (...)
		{
			throw std::runtime_error("unknown exception" + stackString());
		}
	}

	namespace internal
	{


		// an CRTP awaiter used for send operations.
		// when awaited socketSched.send(...) is called.
		// getBuffer() must be implemented 
		// by SendAwaiter.
		template<typename SendAwaiter>
		struct SendAwaiterBase : macoro::basic_traceable
		{
			SockScheduler* mSock = nullptr;
			SessionID mId;
			macoro::stop_token mToken;
			std::exception_ptr mExPtr;
			SendAwaiterBase(SockScheduler* s, SessionID sid, macoro::stop_token&& token)
				: mSock(s)
				, mId(sid)
				, mToken(std::move(token))
			{}

			SendAwaiterBase(SendAwaiterBase&& other)
				: mSock(other.mSock)
				, mId(other.mId)
				, mToken(std::move(other.mToken))
			{
				assert(!other.mExPtr);
			}

			auto& self() { return *(SendAwaiter*)this; }

			bool await_ready() {
				return false;
			}

#ifdef COPROTO_CPP20
			template<typename promise>
			std::coroutine_handle<> await_suspend(std::coroutine_handle<promise> h, std::source_location loc = std::source_location::current())
			{
				set_parent(macoro::detail::get_traceable(h), loc);
				return mSock->send(mId, self().getBuffer(), coroutine_handle<>(h), std::move(mToken)).std_cast();
			}
#endif
			template<typename promise>
			coroutine_handle<> await_suspend(coroutine_handle<promise> h, std::source_location loc = std::source_location::current())
			{
				set_parent(macoro::detail::get_traceable(h), loc);
				return mSock->send(mId, self().getBuffer(), h, std::move(mToken));
			}

			void await_resume()
			{
				if (mExPtr)
				{
					std::vector<std::source_location> stack;
					this->get_call_stack(stack);
					addTraceRethrow(mExPtr, stack);
				}
			}

		};



		// an CRTP awaiter used for recv operations.
		// when awaited socketSched.recv(...) is called.
		// await_resume() and getBuffer() must be implemented 
		// by RecvAwaiter.
		template<typename RecvAwaiter>
		class RecvAwaiterBase : public macoro::basic_traceable
		{
		public:
			SockScheduler* mSock = nullptr;
			SessionID mId;
			macoro::stop_token mToken;
			std::exception_ptr mExPtr;

			RecvAwaiterBase(SockScheduler* s, SessionID sid, macoro::stop_token&& token)
				: mSock(s)
				, mId(sid)
				, mToken(std::move(token))
			{}

			RecvAwaiterBase(RecvAwaiterBase&& other)
				: mSock(other.mSock)
				, mId(other.mId)
				, mToken(std::move(other.mToken))
			{
				assert(!other.mExPtr);
			}

			auto& self() { return *(RecvAwaiter*)this; }

			bool await_ready() { return false; }

#ifdef COPROTO_CPP20
			template<typename promise>
			std::coroutine_handle<> await_suspend(
				std::coroutine_handle<promise> h,
				std::source_location loc = std::source_location::current())
			{
				set_parent(macoro::detail::get_traceable(h), loc);

				return mSock->recv(mId, self().getBuffer(), coroutine_handle<>(h), std::move(mToken)).std_cast();
			}
#endif
			template<typename promise>
			inline coroutine_handle<> await_suspend(
				coroutine_handle<promise> h, 
				std::source_location loc = std::source_location::current())
			{
				set_parent(macoro::detail::get_traceable(h), loc);
				return mSock->recv(mId, self().getBuffer(), h, std::move(mToken));
			}

			void await_resume()
			{
				if (mExPtr)
				{
					std::vector<std::source_location> stack;
					this->get_call_stack(stack);
					addTraceRethrow(mExPtr, stack);
				}
			}
		};

		// this awaiter performs a receive operation.
		// It is used when the thing being received into is an rvalue.
		// We will store the container locally in the awaiter.
		template<typename Container, bool allowResize>
		class MoveRecvAwaiter final : public RecvAwaiterBase<MoveRecvAwaiter<Container, allowResize>>
		{
		public:
			using Base = RecvAwaiterBase<MoveRecvAwaiter<Container, allowResize>>;
			Container mContainer;
			RefRecvBuffer<Container, allowResize> mRef;

			MoveRecvAwaiter(SockScheduler* s, SessionID id, Container&& c, macoro::stop_token&& token)
				: Base(s, id, std::move(token))
				, mContainer(std::forward<Container>(c))
				, mRef(mContainer, &this->mExPtr)
			{}
			MoveRecvAwaiter(SockScheduler* s, SessionID id, macoro::stop_token&& token)
				: Base(s, id, std::move(token))
				, mContainer()
				, mRef(mContainer, &this->mExPtr)
			{}
			//MoveRecvAwaiter() = default;
			MoveRecvAwaiter(const MoveRecvAwaiter&) = delete;
			//MoveRecvAwaiter(MoveRecvAwaiter&&) = delete;

			MoveRecvAwaiter(MoveRecvAwaiter&& m)
				: Base(std::move((Base&)m))
				, mContainer(std::move(m.mContainer))
				, mRef(mContainer, &this->mExPtr)
			{
			}

			RecvBuffer* getBuffer() { return &mRef; }

			Container await_resume()
			{
				RecvAwaiterBase<MoveRecvAwaiter>::await_resume();
				return std::move(mContainer);
			}
		};

		template<typename Container, bool allowResize = true>
		class RefRecvAwaiter final : public RecvAwaiterBase<RefRecvAwaiter<Container, allowResize>>
		{
		public:
			using Base = RecvAwaiterBase<RefRecvAwaiter<Container, allowResize>>;
			Container& mContainer;
			RefRecvBuffer<Container, allowResize> mRef;

			RefRecvAwaiter(SockScheduler* s, SessionID id, Container& t, macoro::stop_token&& token)
				: Base(s, id, std::move(token))
				, mContainer(t)
				, mRef(mContainer, &this->mExPtr)
			{
#ifdef COPROTO_LOGGING
				setName("recv_" + std::to_string(gProtoIdx++));
#endif
			}


			RefRecvAwaiter(RefRecvAwaiter&& m)
				: Base(std::move((Base&)m))
				, mContainer(m.mContainer)
				, mRef(mContainer, &this->mExPtr)
			{
			}

			RecvBuffer* getBuffer() { return &mRef; }
		};


		template<typename Container>
		class RefSendAwaiter final : public SendAwaiterBase<RefSendAwaiter<Container>>
		{
		public:
			using Base = SendAwaiterBase<RefSendAwaiter<Container>>;
			Container& mContainer;

			RefSendAwaiter(SockScheduler* s, SessionID id, Container& t, macoro::stop_token&& token)
				: Base(s, id, std::move(token))
				, mContainer(t)
			{
#ifdef COPROTO_LOGGING
				setName("send_" + std::to_string(gProtoIdx++));
#endif
			}

			RefSendAwaiter(RefSendAwaiter&& m)
				: Base(std::move((Base&)m))
				, mContainer(m.mContainer)
			{
			}


			RefSendBuffer getBuffer()
			{
				return RefSendBuffer(mContainer, &this->mExPtr);
			}

			void await_resume()
			{
				if (this->mExPtr)
				{
					std::vector<std::source_location> stack;
					this->get_call_stack(stack);
					addTraceRethrow(this->mExPtr, stack);
				}
			}
		};


		template<typename Container>
		class MoveSendAwaiter final : public SendAwaiterBase<MoveSendAwaiter<Container>>
		{
		public:
			using Base = SendAwaiterBase<MoveSendAwaiter<Container>>;
			Container mContainer;

			MoveSendAwaiter(SockScheduler* s, SessionID id, Container&& t, macoro::stop_token&& token)
				: Base(s, id, std::move(token))
				, mContainer(std::move(t))
			{
#ifdef COPROTO_LOGGING
				setName("send_" + std::to_string(gProtoIdx++));
#endif
			}

#ifdef COPROTO_CPP20
			template<typename promise>
			std::coroutine_handle<> await_suspend(std::coroutine_handle<promise> h, std::source_location loc = std::source_location::current())
			{
				this->mSock->send(this->mId, getBuffer(), macoro::noop_coroutine(), std::move(this->mToken)).resume();
				return h;
			}
#endif
			template<typename promise>
			coroutine_handle<> await_suspend(coroutine_handle<promise> h, std::source_location loc = std::source_location::current())
			{
				this->mSock->send(this->mId, getBuffer(), macoro::noop_coroutine(), std::move(this->mToken)).resume();
				return h;
			}


			MoveSendAwaiter(MoveSendAwaiter&& m)
				: Base(std::move((Base&)m))
				, mContainer(std::move(m.mContainer))
			{
			}

			MvSendBuffer<Container> getBuffer()
			{
				return MvSendBuffer<Container>(std::move(mContainer), &this->mExPtr);
			}
		};


		class Flush final : macoro::basic_traceable
		{
		public:
			SockScheduler* mSock;
			Flush(SockScheduler* s)
				:mSock(s)
			{}

			bool await_ready() { return false; }

			template<typename promise>
			coroutine_handle<> await_suspend(coroutine_handle<promise> h, std::source_location loc = std::source_location::current())
			{
				set_parent(macoro::detail::get_traceable(h), loc);
				return mSock->flush(h);
			}
#ifdef MACORO_CPP_20
			template<typename promise>
			std::coroutine_handle<> await_suspend(std::coroutine_handle<promise> h, std::source_location loc = std::source_location::current())
			{
				set_parent(macoro::detail::get_traceable(h), loc);
				auto h2 = coroutine_handle<>(h);
				auto f = mSock->flush(h2);
				return f.std_cast();
			}
#endif
			void await_resume() {};
		};

	}
}

