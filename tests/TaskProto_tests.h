#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "Tests.h"

namespace coproto
{

	namespace tests
	{
#ifdef COPROTO_CPP20

		void task_proto_test();
		void task_strSendRecv_Test();
		void task_resultSendRecv_Test();
		void task_typedRecv_Test();
		void task_zeroSendRecv_Test();
		void task_zeroSendRecv_ErrorCode_Test();
		void task_badRecvSize_Test();
		void task_badRecvSize_ErrorCode_Test();
		void task_moveSend_Test();
		void task_throws_Test();
		void task_nestedProtocol_Test();
		void task_nestedProtocol_Throw_Test();
		void task_nestedProtocol_ErrorCode_Test();
		void task_asyncProtocol_Test();
		void task_asyncProtocol_Throw_Test();
		void task_endOfRound_Test();
		void task_errorSocket_Test();
		void task_cancel_send_test();
		void task_cancel_recv_test();
		void task_destroySelf_test();
#else

		inline void task_proto_test(){ throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
		inline void task_strSendRecv_Test(){ throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
		inline void task_resultSendRecv_Test(){ throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
		inline void task_typedRecv_Test(){ throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
		inline void task_zeroSendRecv_Test(){ throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
		inline void task_zeroSendRecv_ErrorCode_Test(){ throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
		inline void task_badRecvSize_Test(){ throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
		inline void task_badRecvSize_ErrorCode_Test(){ throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
		inline void task_moveSend_Test(){ throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
		inline void task_throws_Test(){ throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
		inline void task_nestedProtocol_Test(){ throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
		inline void task_nestedProtocol_Throw_Test(){ throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
		inline void task_nestedProtocol_ErrorCode_Test(){ throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
		inline void task_asyncProtocol_Test(){ throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
		inline void task_asyncProtocol_Throw_Test(){ throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
		inline void task_endOfRound_Test(){ throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
		inline void task_errorSocket_Test(){ throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
		inline void task_cancel_send_test(){ throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
		inline void task_cancel_recv_test(){ throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
		inline void task_destroySelf_test() { throw UnitTestSkipped("CORPOTO_CPP20 not defined."); }
#endif
	}

}