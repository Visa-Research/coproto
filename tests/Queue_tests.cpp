#include "coproto/Common/Queue.h"

#include <list>
#include <set>

namespace coproto
{
	namespace tests
	{

		void Queue_test()
		{
			std::set<u64> qctor, qdtor;
			std::set<u64> lctor, ldtor;
			struct Token
			{
				u64 mI;
				std::set<u64>& dtor;
				Token(u64 i, std::set<u64>& ctor, std::set<u64>& dtor)
					:mI(i),
					dtor(dtor)
				{
					ctor.insert(mI);
				}
				~Token()
				{
					dtor.insert(mI);
				}

				bool operator!=(const Token& o) const { return mI != o.mI; }
			};

			Queue<Token> qInt;
			std::list<Token> lInt;

			auto check = [&]()
				{
					auto b = (lctor != qctor) && (ldtor != qdtor) || (qInt.size() != lInt.size());
					if (b)
						return true;

					auto qIter = qInt.begin();
					auto qEnd = qInt.end();
					auto lIter = lInt.begin();

					while (qIter != qEnd)
					{
						if (*qIter != *lIter)
							return true;

						++qIter;
						++lIter;
					}

					return false;
				};

			qInt.emplace_back(0, qctor, qdtor);
			lInt.emplace_back(0, lctor, ldtor);
			if (check())
				throw COPROTO_RTE;

			qInt.push_back(Token(1, qctor, qdtor));
			lInt.push_back(Token(1, lctor, ldtor));
			if (check())
				throw COPROTO_RTE;

			qInt.emplace_back(2, qctor, qdtor);
			lInt.emplace_back(2, lctor, ldtor);
			if (check())
				throw COPROTO_RTE;

			qInt.pop_front();
			lInt.pop_front();
			if (check())
				throw COPROTO_RTE;

			qInt.pop_front();
			lInt.pop_front();
			if (check())
				throw COPROTO_RTE;

			qInt.pop_front();
			lInt.pop_front();
			if (check())
				throw COPROTO_RTE;

			auto cap = qInt.capacity();
			if (cap != qInt.DefaultCapacity)
				throw COPROTO_RTE;

			for (u64 i = 0; i < 50; ++i)
			{
				if (qInt.size() != i)
					throw COPROTO_RTE;

				qInt.emplace_back(i * 2 + 0, qctor, qdtor);
				lInt.emplace_back(i * 2 + 0, lctor, ldtor);
				qInt.emplace_back(i * 2 + 1, qctor, qdtor);
				lInt.emplace_back(i * 2 + 1, lctor, ldtor);
				if (check())
					throw COPROTO_RTE;
				if (i & 1)
				{
					qInt.pop_front();
					lInt.pop_front();
				}
				else
				{
					auto s = rand() % qInt.size();
					auto qIter = qInt.begin();
					auto lIter = lInt.begin();
					std::advance(qIter, s);
					std::advance(lIter, s);

					qInt.erase(qIter);
					lInt.erase(lIter);
				}
				if (check())
					throw COPROTO_RTE;
			}

			while (qInt.size())
			{
				auto s = rand() % qInt.size();
				auto qIter = qInt.begin();
				auto lIter = lInt.begin();
				std::advance(qIter, s);
				std::advance(lIter, s);

				if (qInt.size() & 1)
				{
					qInt.erase(qIter);
					lInt.erase(lIter);
				}
				else
				{
					qInt.erase(&*qIter);
					lInt.erase(lIter);
				}

				if (check())
					throw COPROTO_RTE;
			}
		}


	}
}
