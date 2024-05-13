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


	struct BadReceiveBufferSize : public std::system_error
	{
		u64 mBufferSize, mReceivedSize;

		BadReceiveBufferSize(u64 bufferSize, u64 receivedSize, std::string msg = {})
			: std::system_error(code::badBufferSize,
				std::string(
					"local buffer size:   " + std::to_string(bufferSize) +
					" bytes\ntransmitted size: " + std::to_string(receivedSize) +
					" bytes\n" + msg
				))
			, mBufferSize(bufferSize)
			, mReceivedSize(receivedSize)
		{}
	};

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
			throw std::system_error(e.code(), e.what() + stackString());
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


		template<typename Container>
		struct MvSendBuffer : public SendOp
		{
			Container mCont;

			MvSendBuffer(Container&& c)
				:mCont(std::forward<Container>(c))
			{
			}

			span<u8> asSpan() override
			{
				return ::coproto::internal::asSpan(mCont);
			}

			void setError(std::exception_ptr e) override
			{
			}

		};



		struct SendProtoBase : macoro::basic_traceable
		{
			SockScheduler* mSock = nullptr;
			SessionID mId;
			std::exception_ptr mExPtr;
			macoro::stop_token mToken;

			SendProtoBase(SockScheduler* s, SessionID sid, macoro::stop_token&& token)
				:mSock(s)
				, mId(sid)
				, mToken(std::move(token))
			{}
			virtual SendBuffer getBuffer() = 0;

			bool await_ready() {
				return false;
			}

#ifdef COPROTO_CPP20

			template<typename promise>
			std::coroutine_handle<> await_suspend(std::coroutine_handle<promise> h, std::source_location loc = std::source_location::current());
#endif
			template<typename promise>
			coroutine_handle<> await_suspend(coroutine_handle<promise> h, std::source_location loc = std::source_location::current());

		};

		class RecvProtoBase : public RecvBuffer
		{
		public:
			SockScheduler* mSock = nullptr;
			SessionID mId;
			macoro::stop_token mToken;

			RecvProtoBase(SockScheduler* s, SessionID sid, macoro::stop_token&& token)
				: mSock(s)
				, mId(sid)
				, mToken(std::move(token))
			{}

			RecvBuffer* getBuffer() {
				return this;
			}

			bool await_ready() { return false; }
#ifdef COPROTO_CPP20
			template<typename promise>
			std::coroutine_handle<> await_suspend(std::coroutine_handle<promise> h, std::source_location loc = std::source_location::current());
#endif
			template<typename promise>
			coroutine_handle<> await_suspend(coroutine_handle<promise> h, std::source_location loc = std::source_location::current());

		};

		template<typename Container, bool allowResize>
		class MoveRecvProto : public RecvProtoBase
		{
		public:
			Container mContainer;

			MoveRecvProto(SockScheduler* s, SessionID id, Container&& c, macoro::stop_token&& token)
				: RecvProtoBase(s, id, std::move(token))
				, mContainer(std::forward<Container>(c))
			{}
			MoveRecvProto(SockScheduler* s, SessionID id, macoro::stop_token&& token)
				: RecvProtoBase(s, id, std::move(token))
			{}
			MoveRecvProto() = default;
			MoveRecvProto(MoveRecvProto&& m)
				: RecvProtoBase(std::move((RecvProtoBase&)m))
				, mContainer(std::move(m.mContainer))
			{
			}
			MoveRecvProto(const MoveRecvProto&) = delete;
			span<u8> asSpan(u64 size) override
			{
				auto buff = tryResize(size, mContainer, allowResize);
				if (buff.size() != size)
				{
					mExPtr = std::make_exception_ptr(BadReceiveBufferSize(buff.size(), size));
				}
				return buff;
			}

			Container await_resume()
			{
				if (mExPtr)
				{
					std::vector<std::source_location> stack;
					get_call_stack(stack);
					addTraceRethrow(mExPtr, stack);
				}
				return std::move(mContainer);
			}
		};

		template<typename Container, bool allowResize = true>
		class RefRecvProto : public RecvProtoBase
		{
		public:
			Container& mContainer;

			RefRecvProto(SockScheduler* s, SessionID id, Container& t, macoro::stop_token&& token)
				: RecvProtoBase(s, id, std::move(token))
				, mContainer(t)
			{
#ifdef COPROTO_LOGGING
				setName("recv_" + std::to_string(gProtoIdx++));
#endif
			}

			span<u8> asSpan(u64 size) override
			{

				auto buff = tryResize(size, mContainer, allowResize);
				if (buff.size() != size)
				{
					mExPtr = std::make_exception_ptr(BadReceiveBufferSize(buff.size(), size));
				}
				return buff;
			}

			void await_resume()
			{

				if (mExPtr)
				{
					std::vector<std::source_location> stack;
					get_call_stack(stack);
					addTraceRethrow(mExPtr, stack);
				}
			}
		};


		template<typename Container>
		class RefSendProto : public SendProtoBase, public SendOp
		{
		public:
			Container& mContainer;

			span<u8> asSpan() override
			{
				return ::coproto::internal::asSpan(mContainer);
			}

			void setError(std::exception_ptr e) override
			{
				mExPtr = e;
			}

			RefSendProto(SockScheduler* s, SessionID id, Container& t, macoro::stop_token&& token)
				: SendProtoBase(s, id, std::move(token))
				, mContainer(t)
			{
#ifdef COPROTO_LOGGING
				setName("send_" + std::to_string(gProtoIdx++));
#endif
			}

			SendBuffer getBuffer() override final
			{
				SendBuffer ret;
				ret.mStorage.setBorrowed(this);
				//ret.mStorage.emplace<RefSendBuffer<Container>>(mContainer);
				return ret;
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


		template<typename Container>
		class MoveSendProto : public SendProtoBase
		{
		public:
			Container mContainer;

			MoveSendProto(SockScheduler* s, SessionID id, Container&& t, macoro::stop_token&& token)
				: SendProtoBase(s, id, std::move(token))
				, mContainer(std::move(t))
			{
#ifdef COPROTO_LOGGING
				setName("send_" + std::to_string(gProtoIdx++));
#endif
			}


			SendBuffer getBuffer() override final
			{
				SendBuffer ret;
				ret.mStorage.emplace<MvSendBuffer<Container>>(std::move(mContainer));
				return ret;
			}

			template<typename P>
			coroutine_handle<> await_suspend(const coroutine_handle<P>& h, std::source_location loc = std::source_location::current())
			{

				if (internal::asSpan(mContainer).size() == 0)
				{
					mExPtr = std::make_exception_ptr(std::system_error(code::sendLengthZeroMsg));
					return h;
				}
				else
				{
					return mSock->send(mId, getBuffer(), h, std::move(mToken), true);
				}
			}

#ifdef COPROTO_CPP20
			template<typename P>
			std::coroutine_handle<> await_suspend(const std::coroutine_handle<P>& h, std::source_location loc = std::source_location::current())
			{
				return await_suspend(static_cast<coroutine_handle<>>(h), loc).std_cast();
			}
#endif


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


		class Flush : macoro::basic_traceable
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


#ifdef COPROTO_CPP20
		template<typename promise>
		inline std::coroutine_handle<> SendProtoBase::await_suspend(std::coroutine_handle<promise> h, std::source_location loc)
		{
			set_parent(macoro::detail::get_traceable(h), loc);
			return mSock->send(mId, getBuffer(), coroutine_handle<>(h), std::move(mToken)).std_cast();
		}
#endif
		template<typename promise>
		inline coroutine_handle<> SendProtoBase::await_suspend(coroutine_handle<promise> h, std::source_location loc)
		{
			set_parent(macoro::detail::get_traceable(h), loc);
			return mSock->send(mId, getBuffer(), h, std::move(mToken));
		}
#ifdef COPROTO_CPP20
		template<typename promise>
		std::coroutine_handle<> RecvProtoBase::await_suspend(std::coroutine_handle<promise> h, std::source_location loc)
		{
			set_parent(macoro::detail::get_traceable(h), loc);

			return mSock->recv(mId, this, coroutine_handle<>(h), std::move(mToken)).std_cast();
		}
#endif
		template<typename promise>
		inline coroutine_handle<> RecvProtoBase::await_suspend(coroutine_handle<promise> h, std::source_location loc)
		{
			set_parent(macoro::detail::get_traceable(h), loc);
			return mSock->recv(mId, this, h, std::move(mToken));
		}

	}
}

