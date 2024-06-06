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
#include "macoro/trace.h"
#include "macoro/stop.h"
#include "coproto/Common/macoro.h"
#include "coproto/Common/Exceptions.h"

namespace coproto
{
	namespace internal
	{

		struct AnyNoCopy
		{
			using Storage = std::aligned_storage_t<256, alignof(std::max_align_t)>;
			Storage mStorage;
			using Deleter = void (*)(AnyNoCopy*);
			Deleter deleter = nullptr;

			AnyNoCopy() = default;
			AnyNoCopy(const AnyNoCopy&) = delete;
			AnyNoCopy(AnyNoCopy&&) = delete;
			~AnyNoCopy()
			{
				if (deleter)
					(*deleter)(this);
			}
			template<typename T>
			enable_if_t<sizeof(T) <= sizeof(Storage), T*>
				emplace(T&& t)
			{
				if (deleter)
					deleter(this);
				using type = typename std::remove_reference<T>::type;
				auto r = new (&mStorage) type(std::forward<T>(t));

				deleter = getDeleter<type>();
				return r;
			}
			template<typename T>
			enable_if_t<(sizeof(T) > sizeof(Storage)), T*>
				emplace(T&& t)
			{
				if (deleter)
					deleter(this);
				using type = typename std::remove_reference<T>::type;
				auto& ptr = *(type**)&mStorage;
				ptr = new type(std::forward<T>(t));
				deleter = getDeleter<type>();
				return ptr;
			}

			template<typename T>
			static enable_if_t<
				std::is_trivially_destructible<T>::value &&
				sizeof(T) <= sizeof(Storage)
				, Deleter> getDeleter()
			{
				return nullptr;
			}

			template<typename T>
			static enable_if_t<
				std::is_trivially_destructible<T>::value == false &&
				sizeof(T) <= sizeof(Storage)
				, Deleter> getDeleter()
			{
				return [](AnyNoCopy* This)
				{
					((T*)&This->mStorage)->~T();
				};
			}

			template<typename T>
			static enable_if_t<(sizeof(T) > sizeof(Storage))
				, Deleter> getDeleter()
			{
				return [](AnyNoCopy* This)
				{
					auto t = *(T**)&This->mStorage;
					delete t;
				};
			}
		};



		template<typename Container>
		enable_if_t<has_size_member_func<Container>::value, u64>
			u8Size(Container& cont)
		{
			return cont.size() * sizeof(typename Container::value_type);
		}

		template<typename Container>
		enable_if_t<
			!has_size_member_func<Container>::value&&
			std::is_trivial<Container>::value, u64>
			u8Size(Container& cont)
		{
			return sizeof(Container);
		}

		template<typename Container>
		enable_if_t<is_trivial_container<Container>::value, span<u8>>
			asSpan(Container& container)
		{
			return span<u8>((u8*)container.data(), u8Size(container));
		}

		template<typename ValueType>
		enable_if_t<std::is_trivial<ValueType>::value, span<u8>>
			asSpan(ValueType& container)
		{
			return span<u8>((u8*)&container, u8Size(container));
		}

		template<typename OTHER>
		enable_if_t<std::is_trivial<OTHER>::value == false && 
			is_trivial_container<OTHER>::value == false, span<u8>>
			asSpan(OTHER& container)
		{
			static_assert(
				std::is_trivial<OTHER>::value ||
				is_trivial_container<OTHER>::value,
				"Coproto does not know how to send & receiver your type. Coproto can send "
				"type T that satisfies \n\n\tstd::is_trivial<T>::value == true\n\tcoproto::is_trivial_container<T>::value == true\n\n"
				"types like int, char, u8 are trivial. Types like std::vector<int> are trivial container. The container must look "
				"like a vector. For a complete specification of coproto::is_trivial_container, see coproto/Common/TypeTraits.h");
		}

		template<typename Container>
		enable_if_t<is_resizable_trivial_container<Container>::value, span<u8>>
			tryResize(u64 size_, Container& container, bool allowResize)
		{
			if (allowResize && (size_ % sizeof(typename Container::value_type)) == 0)
			{
				auto s = size_ / sizeof(typename Container::value_type);
				try {
					container.resize(s);
				}
				catch (...)
				{
					return {};
				}
			}
			return asSpan(container);
		}

		template<typename Container>
		enable_if_t<!is_resizable_trivial_container<Container>::value, span<u8>>
			tryResize(u64, Container& container, bool)
		{
			return asSpan(container);
		}

		// a send buffer wraps a concrete buffer.
		// This buffer allows the caller to store 
		// their data within this object. If they
		// want to know the outcome of the operation,
		// they should set mExPtr. On error, this will 
		// be set with the given exception.
		struct SendBuffer //: macoro::basic_traceable
		{
			std::exception_ptr* mExPtr = nullptr;

			SendBuffer(std::exception_ptr* e)
				:mExPtr(e)
			{}

			virtual ~SendBuffer() {}

			void setError(error_code e) {

				// some buffers are fire and forget. For these
				// they wont have a mExPtr;
				if (mExPtr)
				{
					assert(*mExPtr == nullptr);
					*mExPtr =std::make_exception_ptr(std::system_error(e));
				}
			}

			virtual span<u8> asSpan() = 0;
		};

		// Similar to a send buffer but does not provide storage.
		// this is because recv operations can not be fire and forget.
		struct RecvBuffer //: macoro::basic_traceable
		{
			RecvBuffer(std::exception_ptr* e)
				: mExPtr(e)
			{
				assert(e);
			}

			std::exception_ptr* mExPtr = nullptr;
			void setError(error_code e) {
				if(mExPtr)
					*mExPtr = std::make_exception_ptr(std::system_error(e));
			}
			virtual span<u8> asSpan(u64 resize) = 0;
		};


		// a fire and forget send buffer. The lifetime will be managed by
		// the socket scheduler.
		template<typename Container>
		struct MvSendBuffer : public SendBuffer
		{
			Container mCont;

			MvSendBuffer(Container&& c, std::exception_ptr* e)
				: SendBuffer(e)
				, mCont(std::forward<Container>(c))
			{}

			span<u8> asSpan() override
			{
				return ::coproto::internal::asSpan(mCont);
			}
		};

		struct RefSendBuffer : public SendBuffer
		{
			template<typename Container>
			RefSendBuffer(Container& c, std::exception_ptr* e)
				: SendBuffer(e)
				, mData(coproto::internal::asSpan(c))
			{}

			span<u8> mData;
			span<u8> asSpan() override
			{
				return mData;
			}
		};

		template<typename Container, bool allowResize>
		struct RefRecvBuffer : public RecvBuffer
		{
			RefRecvBuffer(Container& c, std::exception_ptr* e)
				: RecvBuffer(e)
				, mData(c)
			{}

			Container& mData;
			span<u8> asSpan(u64 size) override
			{
				span<u8> r;
				if constexpr (allowResize)
					r = tryResize(size, mData, true);
				else
					r = coproto::internal::asSpan(mData);

				if (r.size() != size)
				{
					COPROTO_ASSERT(mExPtr);
					*mExPtr = std::make_exception_ptr(BadReceiveBufferSize(r.size(), size));
					mExPtr = nullptr;
				}
				return r;
			}
		};

		// an operation that can be queued. Its is simply a callback.
		// it is used to check when all pending operations are completed.
		struct FlushToken
		{
			// the callback
			coroutine_handle<> mHandle;

			FlushToken(coroutine_handle<> h) :mHandle(h) {
				assert(mHandle);
			}
			~FlushToken()
			{
				assert(!mHandle);
			}
		};

	}
}