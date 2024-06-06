#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include <array>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>
#include "coproto/Common/Optional.h"
#include "coproto/Common/Defines.h"

namespace coproto
{

	template<typename T, u64 _DefaultCapacity = 8>
	class Queue
	{
	public:
		const static u64 DefaultCapacity = _DefaultCapacity;

		struct Entry
		{
			bool mEmpty = false;
			T mVal;

			template<typename... Args>
			Entry(Args&&... args)
				: mVal(std::forward<Args>(args)...)
			{}
		};

		struct Block {

			Block(u64 capacity)
			{
				COPROTO_ASSERT_MSG((capacity & (capacity - 1)) == 0, "capacity must be a power of 2");
				mSizeMask = capacity - 1;
			}

			Block* mNext = nullptr;
			u64 mSizeMask;
			u64 mBegin = 0, mEnd = 0;

			u64 occupied() { return mEnd - mBegin; }
			u64 vacant() { return capacity() - occupied(); }
			u64 capacity() { return mSizeMask + 1; }
			Entry* data() { return (Entry*)(this + 1); }
			Entry* begin() { return data() + (mBegin & mSizeMask); }
			Entry* end() { return data() + (mEnd & mSizeMask); }

			Entry& front() { return *begin(); }
			Entry& back() { return data()[(mEnd - 1) & mSizeMask]; }

		};

		std::array<u8, sizeof(Block) + _DefaultCapacity * sizeof(Entry)> mInitalBuff;
		Block* mBegin = nullptr, * mEnd = nullptr;
		u64 mSize = 0;

		Queue() = default;
		Queue(const Queue&) = delete;
		Queue(u64 capacity)
		{
			reserve(capacity);
		}

		~Queue()
		{
			while (size())
				pop_front();
		}

		void reserve(u64 capacity)
		{
			auto v = capacity - 1;
			v |= v >> 1;
			v |= v >> 2;
			v |= v >> 4;
			v |= v >> 8;
			v |= v >> 16;
			v++;

			if (mEnd == nullptr || mEnd->capacity() < v)
				allocateBlock(v);
		}

		void allocateBlock()
		{
			auto nextSize = mEnd ? 2 * mEnd->capacity() : DefaultCapacity;
			allocateBlock(nextSize);
		}

		void allocateBlock(u64 nextSize)
		{

			u8* ptr;
			if (nextSize == DefaultCapacity && mBegin == nullptr)
			{
				ptr = mInitalBuff.data();
			}
			else
			{
				auto allocSize = sizeof(Block) + nextSize * sizeof(T);
				ptr = new u8[allocSize];
			}

			auto blk = new (ptr) Block(nextSize);
			if (mEnd)
				mEnd->mNext = blk;
			else
				mBegin = blk;

			mEnd = blk;
		}

		bool empty() const { return size() == 0; }
		u64 size() const { return mSize; }
		u64 capacity() const {
			return mEnd ? mEnd->capacity() : 0;
		}

		Entry& front_entry()
		{
			COPROTO_ASSERT(mSize);
			return *mBegin->begin();
		}

		T& front()
		{
			COPROTO_ASSERT(mSize);
			return front_entry().mVal;
		}

		T& back()
		{
			COPROTO_ASSERT(mSize);
			return mEnd->back().mVal;
		}

		void pop_front()
		{
			front_entry().~Entry();
			--mSize;
			mBegin->begin()->mEmpty = true;

			while (
				mBegin->occupied() != 0 &&
				mBegin->begin()->mEmpty)
			{
				++mBegin->mBegin;

				if (mBegin->occupied() == 0 &&
					mBegin->mNext)
				{
					auto n = mBegin->mNext;
					if ((u8*)mBegin != mInitalBuff.data())
						delete[](u8*)mBegin;
					mBegin = n;
				}
			}
		}

		void clear()
		{
			while (size())
				pop_front();
		}

		void push_back(const T& t)
		{
			if (mEnd == nullptr || mEnd->vacant() == 0)
				allocateBlock();

			auto ptr = mEnd->end();
			new (ptr) Entry(t);
			++mEnd->mEnd;
			++mSize;
		}

		void push_back(T&& t)
		{
			if (mEnd == nullptr || mEnd->vacant() == 0)
				allocateBlock();

			auto ptr = mEnd->end();
			new (ptr) Entry(std::forward<T>(t));
			++mEnd->mEnd;
			++mSize;
		}

		template<typename... Args>
		void emplace_back(Args&&... args)
		{
			if (mEnd == nullptr || mEnd->vacant() == 0)
				allocateBlock();
			auto ptr = mEnd->end();
			assert(ptr < mEnd->data() + mEnd->capacity());
			new (ptr) Entry(std::forward<Args>(args)...);
			++mEnd->mEnd;
			++mSize;
		}

#define COPROTO_OFFSETOF(s,m) ((::size_t)&reinterpret_cast<char const volatile&>((((s*)0)->m)))

		void erase(T* ptr)
		{
			assert(mSize);
			if (&front() == ptr)
				pop_front();
			else
			{
				Entry& entry = *(Entry*)(((u8*)ptr) - COPROTO_OFFSETOF(Entry, mVal));
				assert(entry.mEmpty == false);
				--mSize;
				entry.mEmpty = true;
			}
		}
	};


	template<typename T, typename... Args>
	std::unique_ptr<T> make_unique(Args&&... args)
	{
		return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
	}

	template <typename T>
	class BlockingQueue
	{
	private:
		struct Impl
		{
			std::mutex              d_mutex;
			std::condition_variable d_condition;
			std::deque<T>           mQueue;
		};

		std::unique_ptr<Impl> mState;
	public:

		BlockingQueue()
			: mState(make_unique<Impl>())
		{}


		BlockingQueue(BlockingQueue&& q)
			: mState(std::move(q.mState))
		{
			q.mState = make_unique<Impl>();
		}

		BlockingQueue& operator=(BlockingQueue&& q)
		{
			mState = (std::move(q.mState));
			q.mState = make_unique<Impl>();
			return *this;
		}


		void clear()
		{
			std::unique_lock<std::mutex> lock(mState->d_mutex);
			mState->mQueue.clear();
		}


		void push(T&& value) {
			{
				std::unique_lock<std::mutex> lock(mState->d_mutex);
				mState->mQueue.emplace_front(std::move(value));
			}
			mState->d_condition.notify_one();
		}

		template<typename... Args>
		void emplace(Args&&... args) {
			{
				std::unique_lock<std::mutex> lock(mState->d_mutex);
				mState->mQueue.emplace_front(std::forward<Args>(args)...);
			}
			mState->d_condition.notify_one();
		}

		T pop() {
			std::unique_lock<std::mutex> lock(mState->d_mutex);
			mState->d_condition.wait(lock, [this] { return !mState->mQueue.empty(); });
			T rc(std::move(mState->mQueue.back()));
			mState->mQueue.pop_back();
			return rc;
		}


		optional<T> tryPop() {
			std::unique_lock<std::mutex> lock(mState->d_mutex);
			if (mState->mQueue.empty())
				return {};

			T rc(std::move(mState->mQueue.back()));
			mState->mQueue.pop_back();
			return rc;
		}


		T popWithSize(u64& size) {
			std::unique_lock<std::mutex> lock(mState->d_mutex);
			mState->d_condition.wait(lock, [this] { return !mState->mQueue.empty(); });
			T rc(std::move(mState->mQueue.back()));
			mState->mQueue.pop_back();
			size = mState->mQueue.size();
			return rc;
		}
	};

	namespace tests
	{
		void Queue_test();
	}

}