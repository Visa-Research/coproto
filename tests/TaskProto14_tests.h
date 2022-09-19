#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


namespace coproto
{

	namespace tests
	{

		void task14_proto_test();
		void task14_strSendRecv_Test();
		void task14_resultSendRecv_Test();
		void task14_typedRecv_Test();
		void task14_zeroSendRecv_Test();
		void task14_zeroSendRecv_ErrorCode_Test();
		void task14_badRecvSize_Test();
		void task14_badRecvSize_ErrorCode_Test();
		void task14_moveSend_Test();
		void task14_throws_Test();
		void task14_nestedProtocol_Test();
		void task14_nestedProtocol_Throw_Test();
		void task14_nestedProtocol_ErrorCode_Test();
		void task14_asyncProtocol_Test();
		void task14_asyncProtocol_Throw_Test();
		void task14_endOfRound_Test();
		void task14_errorSocket_Test();
		void task14_cancel_send_test();
		void task14_cancel_recv_test();
		void task14_timeout_test();
	}

}