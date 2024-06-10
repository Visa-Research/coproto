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
	std::string hexPtr(void* p);

	// a variable sized queue with small buffer optimization and
	// stable iterators. Items can be pushed to the back, popped from
	// the front and eased from the middle using iterators. The queue
	// can also be forward iterated. Capacity is allocated in blocks.
	template<typename T, u64 _DefaultCapacity = 8>
	class Queue
	{
	public:
		const static u64 DefaultCapacity = _DefaultCapacity;

		using Entry = std::optional<T>;

		// a block is somewhat like a vector<Entry>
		// except that the meta data and elements are
		// allocated together, eg memory will look like
		// block,entry,entry,..., entry. There will be 
		// a power of 2 entries. Blocks also form a linked
		// list to the next block with double the capcity.
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


		struct Iterator
		{

			using difference_type = std::ptrdiff_t;
			using value_type = T;

			Block* mBlock = nullptr;
			u64 mIndex = 0;
			T* mVal = nullptr;

			Iterator() = default;
			Iterator(const Iterator&) = default;
			Iterator& operator=(const Iterator&) = default;

			Iterator(Block* block, u64 index)
				: mBlock(block), mIndex(index)
			{
				if (mBlock)
				{
					assert(valid());
					mVal = &entry().value();
				}
			}

			bool valid()
			{
				return
					mBlock != nullptr &&
					mIndex >= mBlock->mBegin &&
					entry().has_value();
			}

			Entry& entry()
			{
				return mBlock->data()[mIndex & mBlock->mSizeMask];
			}

			T& operator*()
			{
				if (valid() == false)
					throw std::out_of_range("Queue<T>::iterator deref invalid.");

				return entry().value();
			}

			T* operator->()
			{
				return &**this;
			}

			Iterator& operator++()
			{
				do {

					++mIndex;
					while (mBlock && mIndex == mBlock->mEnd)
					{
						mBlock = mBlock->mNext;
						if (mBlock)
							mIndex = mBlock->mBegin;
						else
							mIndex = 0;
					}
				} while (mBlock && entry().has_value() == false);

				if (mBlock)
					mVal = &entry().value();
				else
					mVal = nullptr;

				return *this;
			}

			Iterator operator++(int)
			{
				return ++Iterator(*this);
			}

			bool operator==(const Iterator& o) const
			{
				return mBlock == o.mBlock && mIndex == o.mIndex;
			}

			bool operator!=(const Iterator& o) const
			{
				return !(*this == o);
			}


			bool operator==(const std::nullptr_t& o) const
			{
				return mBlock == nullptr;
			}

			bool operator!=(const std::nullptr_t& o) const
			{
				return !(*this == o);
			}

			operator bool() const
			{
				return mBlock != nullptr;
			}

		};

		Iterator begin()
		{
			if (mSize)
				return Iterator{ mBegin, mBegin->mBegin };
			else
				return end();
		}

		Iterator end()
		{
			return Iterator{ nullptr, 0 };
		}

		Iterator back_iterator()
		{
			if (mSize)
				return Iterator(mLast, mLast->mEnd - 1);
			else
				return end();
		}
		// total number of items in the queue.
		u64 mSize = 0;

		// iterators to the first and last block
		Block* mBegin = nullptr, * mLast = nullptr;

		// storage for the small buffer optimization.
		std::aligned_storage_t<sizeof(Block) + _DefaultCapacity * sizeof(Entry), 32> mInitalBuff;


		Queue()
		{
			mBegin = mLast = (Block*)&mInitalBuff;
			new (mBegin) Block(DefaultCapacity);
		}

		Queue(const Queue&) = delete;
		Queue(Queue&&) = delete;

		Queue(u64 capacity)
		{
			reserve(capacity);
		}

		~Queue()
		{
			clear();
		}

		void reserve(u64 capacity)
		{
			// power of 2
			auto v = capacity - 1;
			v |= v >> 1;
			v |= v >> 2;
			v |= v >> 4;
			v |= v >> 8;
			v |= v >> 16;
			v++;

			if (mLast == nullptr || mLast->capacity() < v)
				allocateBlock(v);
		}

		void allocateBlock()
		{
			auto nextSize = mLast ? 2 * mLast->capacity() : DefaultCapacity;
			allocateBlock(nextSize);
		}

		void allocateBlock(u64 nextSize)
		{
			auto allocSize = sizeof(Block) + nextSize * sizeof(Entry);
			auto ptr = ::operator new(allocSize, std::align_val_t{ 32 });
			//std::cout << "new " << hexPtr(ptr) << " " << nextSize << std::endl;

			auto blk = new (ptr) Block(nextSize);
			if (mLast)
				mLast->mNext = blk;
			else
				mBegin = blk;

			mLast = blk;
		}

		bool empty() const { return size() == 0; }
		u64 size() const { return mSize; }
		u64 capacity() const {
			return mLast->capacity();
		}

		Entry& front_entry()
		{
			COPROTO_ASSERT(mBegin->occupied());
			return *mBegin->begin();
		}

		T& front()
		{
			COPROTO_ASSERT(mSize);
			return front_entry().value();
		}

		T& back()
		{
			COPROTO_ASSERT(mSize);
			return mLast->back().value();
		}

		void pop_front()
		{
			assert(mSize && front_entry().has_value());
			front_entry().reset();
			--mSize;

			while (
				mBegin->occupied() != 0 &&
				mBegin->begin()->has_value() == false)
			{
				front_entry().~Entry();
				++mBegin->mBegin;

				if (mBegin->occupied() == 0 &&
					mBegin->mNext)
				{
					auto n = mBegin->mNext;
					if ((u8*)mBegin != (u8*)&mInitalBuff)
					{
						//std::cout << "del " << hexPtr(mBegin) << " " << mBegin->capacity() << std::endl;
						::operator delete((void*)mBegin, std::align_val_t{ 32 });
					}
					mBegin = n;
				}
			}
		}

		void clear()
		{
			while (size())
				pop_front();

			// the last buffer;
			if (mBegin != (Block*)&mInitalBuff)
			{
				//std::cout << "del " << hexPtr(mBegin) << " " << mBegin->capacity() << std::endl;
				::operator delete((void*)mBegin, std::align_val_t{ 32 });
			}
		}

		template<typename ...Args>
		void construct(Entry* ptr, Args&&... args)
		{
			new (ptr) Entry(std::in_place_t{}, std::forward<Args>(args)...);
		}

		void push_back(const T& t)
		{
			if (mLast->vacant() == 0)
				allocateBlock();

			construct(mLast->end(), t);
			++mLast->mEnd;
			++mSize;
		}

		void push_back(T&& t)
		{
			if (mLast->vacant() == 0)
				allocateBlock();

			construct(mLast->end(), std::move(t));
			++mLast->mEnd;
			++mSize;
		}

		template<typename... Args>
		void emplace_back(Args&&... args)
		{
			if (mLast->vacant() == 0)
				allocateBlock();

			construct(mLast->end(), std::forward<Args>(args)...);
			++(mLast->mEnd);
			++mSize;
		}

#define COPROTO_OFFSETOF(s,m) ((::size_t)&reinterpret_cast<char const volatile&>((((s*)0)->m)))

		void erase(T* ptr)
		{
			Block* blk = mBegin;
			while (true)
			{
				auto data = (u8*)blk->data();
				auto end = (u8*)(blk->data() + blk->capacity());
				auto before = (u8*)ptr < data;
				auto after = (u8*)end <= (u8*)ptr;
				if(!before && !after)
					break;

				assert(blk->mNext);
				blk = blk->mNext;
			}

			auto index = ((u8*)ptr - (u8*)blk->data()) / sizeof(Entry);
			Iterator iter(blk, index);
			assert(&*iter == ptr);
			erase(iter);
		}


		void erase(Iterator iter)
		{
			assert(mSize);
			assert(iter.valid());
			if (&front() == &*iter)
				pop_front();
			else
			{
				Entry& entry = iter.entry();
				assert(entry.has_value());
				--mSize;
				entry.reset();
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