#include "coproto/Common/Queue.h"


namespace coproto
{
	namespace tests
	{

		void Queue_test()
		{
			Queue<int> qInt;

			qInt.push_back(0);
			qInt.push_back(1);
			qInt.push_back(3);


			if (qInt.front() != 0 || qInt.size() != 3)
				throw COPROTO_RTE;
			qInt.pop_front();
			if (qInt.front() != 1 || qInt.size() != 2)
				throw COPROTO_RTE;
			qInt.pop_front();
			if (qInt.front() != 3 || qInt.size() != 1)
				throw COPROTO_RTE;
			qInt.pop_front();

			if (qInt.size())
				throw COPROTO_RTE;

			auto cap = qInt.capacity();

			qInt.push_back(0);
			for (u64 i = 1; i < 100; ++i)
			{
				if (qInt.size() != 1)
					throw COPROTO_RTE;

				qInt.push_back(i);

				if (qInt.front() != i - 1 || qInt.size() != 2 || qInt.capacity() != cap)
					throw COPROTO_RTE;

				qInt.pop_front();
			}
			qInt.pop_front();


			for (u64 i = 0; i < 100; ++i)
			{
				if (qInt.size() != i)
					throw COPROTO_RTE;
				qInt.push_back(i);
			}

			for (u64 i = 0; i < 100; ++i)
			{
				if (qInt.size() != 100 - i)
					throw COPROTO_RTE;

				qInt.pop_front();
			}

		}


	}
}
