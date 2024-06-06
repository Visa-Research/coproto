#include "SocketScheduler_tests.h"
#include "coproto/Socket/LocalAsyncSock.h"
#include "coproto/Socket/BufferingSocket.h"
#include <vector>
#include "macoro/thread_pool.h"
#include "tests/Tests.h"

namespace coproto
{
	namespace tests
	{
		template<typename T>
		enable_if_t<std::is_trivial<typename std::remove_reference<T>::type>::value> push(T&& t, std::vector<u8>& v)
		{
			auto i = v.size();
			v.resize(v.size() + sizeof(t));
			memcpy(v.data() + i, &t, sizeof(t));
		}
		template<typename T>
		enable_if_t<std::is_trivial<typename std::remove_reference<T>::type>::value> pop(T& t, std::vector<u8>& v)
		{
			assert(sizeof(t) <= v.size());
			memcpy(&t, v.data(), sizeof(t));
			memmove(v.data(), v.data() + sizeof(t), v.size() - sizeof(t));
			v.resize(v.size() - sizeof(t));
		}
		template<typename T>
		enable_if_t<std::is_trivial<typename std::remove_reference<T>::type>::value> popEq(const T& e, std::vector<u8>& v)
		{
			T t;
			pop(t, v);
			if (t != e)
				throw MACORO_RTE_LOC;

		}
		void SocketScheduler_basicSend_test()
		{

			auto s = LocalAsyncSocket::makePair();

			Socket& sender = s[0];
			auto& sImpl = sender.mImpl;

			std::vector<u8> sendBuff(14);


			for (u64 i = 0; i < sendBuff.size(); ++i)
				sendBuff[i] = i;

			internal::RefSendAwaiter<std::vector<u8>> p(sImpl.get(), sender.mId, sendBuff, {});
			bool sendDone = false;
			auto sendTask = [](bool& sendDone) -> task<void>
			{
				MC_BEGIN(task<>, &sendDone);
				sendDone = true;
				MC_AWAIT(macoro::suspend_always{});
				MC_END();
			}(sendDone);

			sImpl->send(sender.mId, p.getBuffer(), sendTask.handle(), {}).resume();
			if (sendDone)
				throw MACORO_RTE_LOC;

			u32 size = 0, slot = 0;
			SessionID sid = sender.mId;

			bool recvDone = false;
			auto recvTask = [](bool& done) -> task<void>
			{
				MC_BEGIN(task<>, &done);
				done = true;
				MC_AWAIT(macoro::suspend_always{});
				MC_END();
			}(recvDone);


			std::vector<u8> recvBuffer(sizeof(u32) * 4 + sizeof(SessionID::mVal) + sendBuff.size());
			auto awaiter = s[1].mSock->recv(recvBuffer);
			if (awaiter.await_ready())
				throw MACORO_RTE_LOC;

			awaiter.await_suspend(recvTask.handle()).resume();

			pop(size, recvBuffer);
			if (size != 0)
				throw MACORO_RTE_LOC;
			pop(slot, recvBuffer);
			if (slot != 1)
				throw MACORO_RTE_LOC;

			pop(sid.mVal, recvBuffer);
			if (sid != sender.mId)
				throw MACORO_RTE_LOC;

			pop(size, recvBuffer);
			if (size != sendBuff.size())
				throw MACORO_RTE_LOC;

			pop(slot, recvBuffer);
			if (slot != 1)
				throw MACORO_RTE_LOC;

			if (recvBuffer != sendBuff)
				throw MACORO_RTE_LOC;

			if (!sendDone)
				throw MACORO_RTE_LOC;

			if (!recvDone)
				throw MACORO_RTE_LOC;
		}


		void SocketScheduler_basicRecv_test()
		{

			auto s = LocalAsyncSocket::makePair();

			Socket& sender = s[0];
			auto& sImpl = sender.mImpl;

			std::vector<u8> sendBuff;
			std::vector<u8> recvBuffer;

			internal::RefRecvAwaiter<std::vector<u8>, true> p(sImpl.get(), sender.mId, recvBuffer, {});
			bool sendDone = false;
			auto sendTask = [](bool& done) -> task<void>
			{
				MC_BEGIN(task<>, &done);
				done = true;
				MC_AWAIT(macoro::suspend_always{});
				MC_END();
			}(sendDone);

			bool recvDone = false;
			auto recvTask = [](bool& done) -> task<void>
			{
				MC_BEGIN(task<>, &done);
				done = true;
				MC_AWAIT(macoro::suspend_always{});
				MC_END();
			}(recvDone);

			sImpl->recv(sender.mId, p.getBuffer(), recvTask.handle(), {}).resume();
			if (sendDone)
				throw MACORO_RTE_LOC;

			u32 size = 0, slot = 0;
			SessionID sid = sender.mId;

			push(size, sendBuff);
			push(slot, sendBuff);
			push(sid.mVal, sendBuff);

			size = 14;
			push(size, sendBuff);
			push(slot, sendBuff);
			for (u64 i = 0; i < size; ++i)
				sendBuff.push_back(i);

			auto awaiter = s[1].mSock->send(sendBuff);
			if (awaiter.await_ready())
				throw MACORO_RTE_LOC;

			awaiter.await_suspend(sendTask.handle()).resume();

			if (recvBuffer.size() != size)
				throw MACORO_RTE_LOC;

			for (u64 i = 0; i < recvBuffer.size(); ++i)
				if (recvBuffer[i] != i)
					throw MACORO_RTE_LOC;

			if (!sendDone)
				throw MACORO_RTE_LOC;

			if (!recvDone)
				throw MACORO_RTE_LOC;
		}

		void SocketScheduler_cancelSend_test()
		{

			auto s = LocalAsyncSocket::makePair();

			Socket& sender = s[0];
			auto& sImpl = sender.mImpl;

			std::vector<u8> sendBuff(14);


			for (u64 i = 0; i < sendBuff.size(); ++i)
				sendBuff[i] = i;

			auto tt = [](bool& done) -> task<void>
			{
				MC_BEGIN(task<>, &done);
				done = true;
				MC_AWAIT(macoro::suspend_always{});
				MC_END();
			};

			bool sendDone0 = false;
			auto sendTask0 = tt(sendDone0);
			bool sendDone1 = false;
			auto sendTask1 = tt(sendDone1);

			macoro::stop_source src0, src1;
			auto token0 = src0.get_token();
			auto token1 = src1.get_token();

			sImpl->enableLogging();

			internal::RefSendAwaiter<std::vector<u8>> p0(sImpl.get(), sender.mId, sendBuff, {});
			sImpl->send(sender.mId, p0.getBuffer(), sendTask0.handle(), std::move(token0)).resume();
			if (sendDone0)
				throw MACORO_RTE_LOC;

			internal::RefSendAwaiter<std::vector<u8>> p1(sImpl.get(), sender.mId, sendBuff, {});
			sImpl->send(sender.mId, p1.getBuffer(), sendTask1.handle(), std::move(token1)).resume();
			if (sendDone1)
				throw MACORO_RTE_LOC;

			src1.request_stop();

			if (!sendDone1 || !p1.mExPtr)
				throw MACORO_RTE_LOC;

			try {
				std::rethrow_exception(p1.mExPtr);
			}
			catch (std::system_error& ex)
			{
				auto ec = ex.code();
				if (ec != code::operation_aborted)
					throw;
			}

			if (sendDone0)
				throw MACORO_RTE_LOC;


			src0.request_stop();

			if (!sendDone0 || !p0.mExPtr)
				throw MACORO_RTE_LOC;

			try {
				std::rethrow_exception(p0.mExPtr);
			}
			catch (std::system_error& ex)
			{
				auto ec = ex.code();
				if (ec != code::operation_aborted)
					throw;
			}

		}

		void SocketScheduler_cancelRecv_test()
		{

			auto s = LocalAsyncSocket::makePair();

			Socket& recver = s[0];
			auto& rImpl = s[1].mImpl;
			auto& sImpl = recver.mImpl;

			std::vector<u8> recvBuff(14);


			for (u64 i = 0; i < recvBuff.size(); ++i)
				recvBuff[i] = i;

			internal::RefRecvAwaiter<std::vector<u8>> p0(sImpl.get(), recver.mId, recvBuff, {});
			internal::RefRecvAwaiter<std::vector<u8>> p1(sImpl.get(), recver.mId, recvBuff, {});
			auto tt = [](bool& done) -> task<void>
			{
				MC_BEGIN(task<>, &done);
				done = true;
				MC_AWAIT(macoro::suspend_always{});
				MC_END();
			};

			bool recvDone0 = false;
			auto recvTask0 = tt(recvDone0);
			bool recvDone1 = false;
			auto recvTask1 = tt(recvDone1);

			macoro::stop_source src0, src1;
			auto token0 = src0.get_token();
			auto token1 = src1.get_token();

			sImpl->recv(recver.mId, p0.getBuffer(), recvTask0.handle(), std::move(token0)).resume();
			if (recvDone0)
				throw MACORO_RTE_LOC;
			sImpl->recv(recver.mId, p1.getBuffer(), recvTask1.handle(), std::move(token1)).resume();
			if (recvDone1)
				throw MACORO_RTE_LOC;

			src1.request_stop();

			if (!recvDone1 || !p1.mExPtr)
				throw MACORO_RTE_LOC;

			try {
				std::rethrow_exception(p1.mExPtr);
			}
			catch (std::system_error& ex)
			{
				if (ex.code() != code::operation_aborted)
					throw;
			}

			if (recvDone0)
				throw MACORO_RTE_LOC;


			src0.request_stop();

			if (!recvDone0 || !p0.mExPtr)
				throw MACORO_RTE_LOC;

			try {
				std::rethrow_exception(p0.mExPtr);
			}
			catch (std::system_error& ex)
			{
				if (ex.code() != code::operation_aborted)
					throw;
			}

			sImpl->mRecvStatus = internal::SockScheduler::Status::Closed;
			sImpl->mSendStatus = internal::SockScheduler::Status::Closed;
			rImpl->mRecvStatus = internal::SockScheduler::Status::Closed;
			rImpl->mSendStatus = internal::SockScheduler::Status::Closed;
		}

		void SocketScheduler_restoreSend_test()
		{
			throw UnitTestSkipped("not implemented");
			// header 0: 8
			// SID     : 16     24
			// header 1: 8      32
			// body   1: 14     46
			// header 2: 8      54
			// body   2: 14     68

			// receive enough bytes to put is mid buffer.
			// make sure that the socket can get back to 
			// a good state.
			for (u64 recvSize : { 4, 12, 30, 40})
			{
				auto s = LocalAsyncSocket::makePair();

				Socket& sender = s[0];
				auto& sImpl = sender.mImpl;

				std::vector<u8> sendBuff0(14);
				std::vector<u8> sendBuff1(14);


				for (u64 i = 0; i < sendBuff0.size(); ++i)
				{
					sendBuff0[i] = i;
					sendBuff1[i] = i * 2;
				}

				internal::RefSendAwaiter<std::vector<u8>> p0(sImpl.get(), sender.mId, sendBuff0, {});
				internal::RefSendAwaiter<std::vector<u8>> p1(sImpl.get(), sender.mId, sendBuff1, {});
				auto tt = [](bool& done) -> task<void>
				{
					MC_BEGIN(task<>, &done);
					done = true;
					MC_AWAIT(macoro::suspend_always{});
					MC_END();
				};

				bool sendDone0 = false;
				auto sendTask0 = tt(sendDone0);
				bool sendDone1 = false;
				auto sendTask1 = tt(sendDone1);
				bool recvDone0 = false;
				auto recvTask0 = tt(recvDone0);

				macoro::stop_source src0, src1;
				auto token0 = src0.get_token();
				auto token1 = src1.get_token();

				sImpl->send(sender.mId, p0.getBuffer(), sendTask0.handle(), std::move(token0)).resume();
				if (sendDone0)
					throw MACORO_RTE_LOC;
				sImpl->send(sender.mId, p1.getBuffer(), sendTask1.handle(), std::move(token1)).resume();
				if (sendDone1)
					throw MACORO_RTE_LOC;

				std::vector<u8> exp;

				// size, slot, sid
				push(0, exp);
				push(1, exp);
				push(sender.mId.mVal, exp);

				// if we have great more than 24 bytes, then
				// the sender has started sending the first buffer. 
				// They have to finish it...
				if (recvSize > 24)
				{
					// size, slot, body 1
					push(u32(sendBuff0.size()), exp);
					push(1, exp);
					exp.insert(exp.end(), sendBuff0.begin(), sendBuff0.end());
				}

				// size, slot, body 2
				push(u32(sendBuff1.size()), exp);
				push(1, exp);
				exp.insert(exp.end(), sendBuff1.begin(), sendBuff1.end());


				// do a partial read
				std::vector<u8> recvBuffer(recvSize);
				auto recvAwaiter0 = s[1].mSock->recv(recvBuffer);
				recvAwaiter0.await_suspend(recvTask0.handle()).resume();

				// make sure the recv completed.
				if (!recvAwaiter0.mEc || recvAwaiter0.mEc.value() || recvDone0 == false)
					throw MACORO_RTE_LOC;
				if (std::equal(recvBuffer.begin(), recvBuffer.end(), exp.begin()) == false)
					throw MACORO_RTE_LOC;
				auto exp2 = span<u8>(exp).subspan(recvSize);

				// cancel the send op.
				src0.request_stop();

				// make sure send completed.
				if (!sendDone0 || !p0.mExPtr)
					throw MACORO_RTE_LOC;
				try { std::rethrow_exception(p0.mExPtr); }
				catch (std::system_error& ex) {
					if (ex.code() != code::operation_aborted)
						throw;
				}

				// get the rest. Should be the unsent
				bool recvDone1 = false;
				auto recvTask1 = tt(recvDone1);

				recvBuffer.resize(exp2.size());
				auto recvAwaiter1 = s[1].mSock->recv(recvBuffer);
				if (recvAwaiter1.await_ready())
					throw MACORO_RTE_LOC;

				recvAwaiter1.await_suspend(recvTask1.handle()).resume();

				// make sure the second send completed
				if (!sendDone1 || p1.mExPtr)
					throw MACORO_RTE_LOC;

				// make sure the receive completed
				if (!recvAwaiter1.mEc || recvAwaiter1.mEc.value() || recvDone1 == false)
					throw MACORO_RTE_LOC;
				if (std::equal(recvBuffer.begin(), recvBuffer.end(), exp2.begin()) == false)
					throw MACORO_RTE_LOC;
			}
		}

		void SocketScheduler_restoreRecv_test()
		{
			// header 0:  8		 8
			// SID     : 16     24
			// header 1:  8     32
			// body   1: 14     46
			// header 2:  8     54
			// body   2: 14     68

			// send enough bytes to put is mid buffer.
			// make sure that the socket can get back to 
			// a good state.
			for (u64 sendSize : {40})
			{

				auto s = LocalAsyncSocket::makePair();

				Socket& sender = s[0];
				auto& sImpl = sender.mImpl;

				std::vector<u8> sendBuff;
				std::vector<u8> recvBuffer0, recvBuffer1;

				macoro::stop_source src;
				auto token = src.get_token();

				internal::RefRecvAwaiter<std::vector<u8>, true> p0(sImpl.get(), sender.mId, recvBuffer0, {});
				internal::RefRecvAwaiter<std::vector<u8>, true> p1(sImpl.get(), sender.mId, recvBuffer1, {});
				auto tt = [](bool& done) -> task<void>
				{
					MC_BEGIN(task<>, &done);
					done = true;
					MC_AWAIT(macoro::suspend_always{});
					MC_END();
				};

				bool sendDone0 = false;
				auto sendTask0 = tt(sendDone0);
				bool sendDone1 = false;
				auto sendTask1 = tt(sendDone1);
				bool recvDone0 = false;
				auto recvTask0 = tt(recvDone0);
				bool recvDone1 = false;
				auto recvTask1 = tt(recvDone1);

				sImpl->recv(sender.mId, p0.getBuffer(), recvTask0.handle(), std::move(token)).resume();
				sImpl->recv(sender.mId, p1.getBuffer(), recvTask1.handle(), {}).resume();
				if (recvDone0)
					throw MACORO_RTE_LOC;

				u32 size = 0, slot = 0;
				SessionID sid = sender.mId;

				push(size, sendBuff);
				push(slot, sendBuff);
				push(sid.mVal, sendBuff);

				size = 14;
				push(size, sendBuff);
				push(slot, sendBuff);
				for (u64 i = 0; i < size; ++i)
					sendBuff.push_back(i);

				push(size, sendBuff);
				push(slot, sendBuff);
				for (u64 i = 0; i < size; ++i)
					sendBuff.push_back(i * 2);

				auto sendAwaiter0 = s[1].mSock->send(span<u8>(sendBuff).subspan(0, sendSize));
				if (sendAwaiter0.await_ready())
					throw MACORO_RTE_LOC;

				sendAwaiter0.await_suspend(sendTask0.handle()).resume();

				if (recvBuffer0.size() != size)
					throw MACORO_RTE_LOC;

				if (!sendDone0 || !sendAwaiter0.mEc || *sendAwaiter0.mEc)
					throw MACORO_RTE_LOC;

				src.request_stop();

				if (!recvDone0)
					throw MACORO_RTE_LOC;

				auto rem = span<u8>(sendBuff).subspan(sendSize);
				auto sendAwaiter1 = s[1].mSock->send(rem);
				if (sendAwaiter1.await_ready())
					throw MACORO_RTE_LOC;

				sendAwaiter1.await_suspend(sendTask1.handle()).resume();

				if (!recvDone1)
					throw MACORO_RTE_LOC;
				if (!sendDone1)
					throw MACORO_RTE_LOC;

				for (u64 i = 0; i < recvBuffer1.size(); ++i)
					if (recvBuffer1[i] != 2 * i)
						throw MACORO_RTE_LOC;

			}
		}

		void SocketScheduler_closeSend_test()
		{


			// header 0: 8
			// SID     : 16     24
			// header 1: 8      32
			// body   1: 14     46
			// header 2: 8      54
			// body   2: 14     68

			// receive enough bytes to put is mid buffer.
			// make sure that the socket can get back to 
			// a good state.
			for (bool senderClose : {true, false})
			{
				for (u64 recvSize : { 4, 12, 30, 40})
				{
					auto s = LocalAsyncSocket::makePair();

					Socket& sender = s[0];
					auto& sImpl = sender.mImpl;

					std::vector<u8> sendBuff0(14);
					std::vector<u8> sendBuff1(14);

					for (u64 i = 0; i < sendBuff0.size(); ++i)
					{
						sendBuff0[i] = i;
						sendBuff1[i] = i * 2;
					}

					internal::RefSendAwaiter<std::vector<u8>> p0(sImpl.get(), sender.mId, sendBuff0, {});
					internal::RefSendAwaiter<std::vector<u8>> p1(sImpl.get(), sender.mId, sendBuff1, {});
					auto tt = [](bool& done) -> task<void>
					{
						MC_BEGIN(task<>, &done);
						done = true;
						MC_AWAIT(macoro::suspend_always{});
						MC_END();
					};

					bool sendDone0 = false;
					auto sendTask0 = tt(sendDone0);
					bool sendDone1 = false;
					auto sendTask1 = tt(sendDone1);
					bool recvDone0 = false;
					auto recvTask0 = tt(recvDone0);

					//macoro::stop_source src0, src1;
					//auto token0 = src0.get_token();
					//auto token1 = src1.get_token();

					sImpl->send(sender.mId, p0.getBuffer(), sendTask0.handle(), {}).resume();
					if (sendDone0)
						throw MACORO_RTE_LOC;
					sImpl->send(sender.mId, p1.getBuffer(), sendTask1.handle(), {}).resume();
					if (sendDone1)
						throw MACORO_RTE_LOC;

					// do a partial read
					std::vector<u8> recvBuffer(recvSize);
					auto recvAwaiter0 = s[1].mSock->recv(recvBuffer);
					recvAwaiter0.await_suspend(recvTask0.handle()).resume();

					// make sure the recv completed.
					if (!recvAwaiter0.mEc || recvAwaiter0.mEc.value() || recvDone0 == false)
						throw MACORO_RTE_LOC;

					// cancel the send op.
					if (senderClose)
						s[0].close();
					else
						s[1].close();

					// make sure send completed.
					if (!sendDone0 || !p0.mExPtr)
						throw MACORO_RTE_LOC;
					try { std::rethrow_exception(p0.mExPtr); }
					catch (std::system_error& ex) {
						if (senderClose)
						{
							if (ex.code() != code::closed)
								throw;
						}
						else
							if (ex.code() != code::remoteClosed)
								throw;
					}

					// make sure send completed.
					if (!sendDone1 || !p1.mExPtr)
						throw MACORO_RTE_LOC;
					try { std::rethrow_exception(p1.mExPtr); }
					catch (std::system_error& ex) {
						if (ex.code() != code::cancel)
							throw;
					}
				}
			}
		}

		void SocketScheduler_closeRecv_test()
		{


			// header 0: 8
			// SID     : 16     24
			// header 1: 8      32
			// body   1: 14     46
			// header 2: 8      54
			// body   2: 14     68

			// receive enough bytes to put is mid buffer.
			// make sure that the socket can get back to 
			// a good state.
			for (bool recverClose : {true, false})
			{
				for (u64 recvSize : { 4, 12, 30, 40})
				{
					auto s = LocalAsyncSocket::makePair();

					Socket& recver = s[0];
					auto& sImpl = recver.mImpl;

					std::vector<u8> sendBuff0(14);
					std::vector<u8> sendBuff1(14);

					for (u64 i = 0; i < sendBuff0.size(); ++i)
					{
						sendBuff0[i] = i;
						sendBuff1[i] = i * 2;
					}

					internal::RefRecvAwaiter<std::vector<u8>> p0(sImpl.get(), recver.mId, sendBuff0, {});
					internal::RefRecvAwaiter<std::vector<u8>> p1(sImpl.get(), recver.mId, sendBuff1, {});
					auto tt = [](bool& done) -> task<void>
					{
						MC_BEGIN(task<>, &done);
						done = true;
						MC_AWAIT(macoro::suspend_always{});
						MC_END();
					};

					bool recvDone0 = false;
					auto recvTask0 = tt(recvDone0);
					bool recvDone1 = false;
					auto recvTask1 = tt(recvDone1);
					bool sendDone0 = false;
					auto sendTask0 = tt(sendDone0);

					sImpl->recv(recver.mId, p0.getBuffer(), recvTask0.handle(), {}).resume();
					if (recvDone0)
						throw MACORO_RTE_LOC;
					sImpl->recv(recver.mId, p1.getBuffer(), recvTask1.handle(), {}).resume();
					if (recvDone1)
						throw MACORO_RTE_LOC;

					// do a partial read

					u32 size = 0, slot = 0;
					SessionID sid = recver.mId;
					std::vector<u8> sendBuff;
					push(size, sendBuff);
					push(slot, sendBuff);
					push(sid.mVal, sendBuff);

					size = 14;
					push(size, sendBuff);
					push(slot, sendBuff);
					for (u64 i = 0; i < size; ++i)
						sendBuff.push_back(i);

					push(size, sendBuff);
					push(slot, sendBuff);
					for (u64 i = 0; i < size; ++i)
						sendBuff.push_back(i * 2);

					auto sendAwaiter0 = s[1].mSock->send(span<u8>(sendBuff).subspan(0, recvSize));
					sendAwaiter0.await_suspend(sendTask0.handle()).resume();

					// make sure the recv completed.
					if (!sendAwaiter0.mEc || sendAwaiter0.mEc.value() || sendDone0 == false)
						throw MACORO_RTE_LOC;

					// cancel the send op.
					if (recverClose)
						s[0].close();
					else
						s[1].close();

					// make sure send completed.
					if (!recvDone0 || !p0.mExPtr)
						throw MACORO_RTE_LOC;
					try { std::rethrow_exception(p0.mExPtr); }
					catch (std::system_error& ex) {
						if (recverClose)
						{
							if (ex.code() != code::closed)
								throw;
						}
						else
							if (ex.code() != code::remoteClosed)
								throw;
					}

					// make sure send completed.
					if (!recvDone1 || !p1.mExPtr)
						throw MACORO_RTE_LOC;
					try { std::rethrow_exception(p1.mExPtr); }
					catch (std::system_error& ex) {
						auto ec = ex.code();
						auto exp = code::cancel;
							
						if (ec != exp)
							throw;
						//if (recverClose)
						//{
						//}
						//else
						//	if (ec != code::remoteClosed)
						//		throw;
					}
				}
			}
		}

		void SocketScheduler_cForkSend_test()
		{

			//// header 0: 8
			//// SID     : 16     24
			//// header 1: 8      32
			//// body   1: 14     46
			//// header 2: 8      54
			//// body   2: 14     68

			//// receive enough bytes to put is mid buffer.
			//// make sure that the socket can get back to 
			//// a good state.
			//for (u64 recvSize : { 40 })
			//{
			//	auto s = LocalAsyncSocket::makePair();

			//	auto& sender0 = s[0];
			//	auto& recver0 = s[1];

			//	auto sender1 = sender0.fork();
			//	auto recver1 = recver0.fork();
			//	auto& sImpl = sender0.mImpl;

			//	std::vector<u8> sendBuff0(14);
			//	std::vector<u8> sendBuff1(14);

			//	for (u64 i = 0; i < sendBuff0.size(); ++i)
			//	{
			//		sendBuff0[i] = i;
			//		sendBuff1[i] = i * 2;
			//	}

			//	auto tt = [](bool& done) -> task<void>
			//	{
			//		done = true;
			//		co_await std::suspend_always{};
			//	};

			//	bool sendDone0 = false;
			//	auto sendTask0 = tt(sendDone0);
			//	bool sendDone1 = false;
			//	auto sendTask1 = tt(sendDone1);
			//	bool recvDone0 = false;
			//	auto recvTask0 = tt(recvDone0);

			//	//macoro::stop_source src0, src1;
			//	//auto token0 = src0.get_token();
			//	//auto token1 = src1.get_token();
			//	auto p0 = sender0.send(sendBuff0);
			//	p0.await_suspend(sendTask0.handle).resume();
			//	if (sendDone0)
			//		throw MACORO_RTE_LOC;
			//	auto p1 = sender1.send(sendBuff1);
			//	p1.await_suspend(sendTask1.handle()).resume();
			//	if (sendDone1)
			//		throw MACORO_RTE_LOC;

			//	// do a partial read
			//	std::vector<u8> recvBuffer(recvSize);
			//	auto recvAwaiter0 = recver0.mSock->recv(recvBuffer);
			//	recvAwaiter0.await_suspend(recvTask0.handle()).resume();

			//	// make sure the recv completed.
			//	if (!recvAwaiter0.mEc || recvAwaiter0.mEc.value() || recvDone0 == false)
			//		throw MACORO_RTE_LOC;


			//	sender0.closeFork();


			//}

		}

		void SocketScheduler_cForkRecv_test()
		{
		}

		void SocketScheduler_pendingRecvClose_test()
		{

			//	r0 <----
			//           <----- s1
			//  s0 ----> x

			auto s = LocalAsyncSocket::makePair();

			Socket recver0 = s[0];
			Socket sender0 = s[1];
			Socket recver1 = s[0].fork();
			Socket sender1 = s[1].fork();

			auto tt = [](bool& done) -> task<void>
			{
				MC_BEGIN(task<>, &done);
				done = true;
				MC_AWAIT(macoro::suspend_always{});
				MC_END();
			};

			std::vector<u8> recvBuff0(10);
			auto recv0 = recver0.recv(recvBuff0);
			bool recv0Done = false;
			auto r0 = tt(recv0Done);
			recv0.await_suspend(r0.handle()).resume();

			if (recv0Done)
				throw MACORO_RTE_LOC;

			std::vector<u8> sendBuff1(11);
			auto send1 = sender1.send(sendBuff1);
			bool send1Done = false;
			auto s1 = tt(send1Done);
			send1.await_suspend(s1.handle()).resume();

			if (send1Done)
				throw MACORO_RTE_LOC;


			s[0].mSock->mImpl->mDebugErrorInjector = [] {return code::DEBUG_ERROR; };
			std::vector<u8> sendBuff0(10);
			auto send0 = recver0.send(sendBuff0);
			bool send0Done = false;
			auto s0 = tt(send0Done);
			send0.await_suspend(s0.handle()).resume();

			if (!send0Done)
				throw MACORO_RTE_LOC;
			if (!recv0Done)
				throw MACORO_RTE_LOC;
			if (!send1Done)
				throw MACORO_RTE_LOC;

		}

		
		// What happens if we dont send the first message as
		// 
		//    0, <new-slot-id>, <session-ID>
		//
		// but instead try to send a data message
		// 
		//    <size>, <slot-id>, <msg>
		// 
		// This should cause the socket to close as this is 
		// an invalid message.
		//
		void SocketScheduler_badFirstMgs_test()
		{
			BufferingSocket sock;

			auto sid = SessionID::root();
			std::vector<u8> buffer;
			// <size>
			push(u32(1), buffer);

			// <slot-id>
			push(u32(0), buffer);

			// <session-id>
			push(u64(0), buffer);
			push(u64(0), buffer);

			auto tt = [&]()->task<> {
				MC_BEGIN(task<>, &, b = std::array<u8, 10>());

				MC_AWAIT(sock.recv(b));

				MC_END();
			};
			auto task = macoro::make_blocking(tt());

			sock.processInbound(buffer);

			try {
				task.get();
				throw COPROTO_RTE;
			}
			catch (std::system_error e)
			{
				if (e.code() != code::badCoprotoMessageHeader)
					throw;
			}


		}

		// What happens if we try to initialize a new slot twice.
		// Basically if we send the following twice.
		// 
		//    0, <slot-id>, <session-ID>
		//
		// This is invalid as currently the only meta message that
		// is supported is to initialize a slot with a new <slot-id>.
		//
		void SocketScheduler_repeatInitSlot_test()
		{

			BufferingSocket sock;

			auto sid = SessionID::root();
			std::vector<u8> buffer;
			// <size>
			push(u32(0), buffer);

			// <slot-id>
			push(u32(0), buffer);

			// <session-id>
			push(u64(0), buffer);
			push(u64(0), buffer);

			auto tt = [&]()->task<> {
				MC_BEGIN(task<>, &, b = std::array<u8, 10>());

				MC_AWAIT(sock.recv(b));

				MC_END();
			};
			auto task = tt() | macoro::make_blocking();

			sock.processInbound(buffer);
			sock.processInbound(buffer);

			try {
				task.get();
				throw COPROTO_RTE;
			}
			catch (std::system_error e)
			{
				if (e.code() != code::badCoprotoMessageHeader)
					throw;
			}


		}


		// What happens we try to receive a message on a slot that
		// has not been initialized.
		// 
		//    <size>, <unkown-slot-id>, <msg>
		//
		// This is invalid as the slot must first be initialized. The socket
		// should close with an error.
		//
		void SocketScheduler_badSlotSend_test()
		{

			BufferingSocket sock;

			auto sid = SessionID::root();
			std::vector<u8> buffer;
			// <size>
			push(u32(0), buffer);

			// <slot-id>
			push(u32(0), buffer);

			// <session-id>
			push(u64(0), buffer);
			push(u64(0), buffer);


			// <size>
			push(u32(16), buffer);

			// <unknown-slot-id>
			push(u32(1), buffer);

			// <session-id>
			push(u64(0), buffer);
			push(u64(0), buffer);

			auto tt = [&]()->task<> {
				MC_BEGIN(task<>, &, b = std::array<u8, 10>());

				MC_AWAIT(sock.recv(b));

				MC_END();
			};
			auto task = tt() | macoro::make_blocking();

			sock.processInbound(buffer);

			try {
				task.get();
				throw COPROTO_RTE;
			}
			catch (std::system_error e)
			{
				if (e.code() != code::badCoprotoMessageHeader)
					throw;
			}

		}

		void SocketScheduler_executor_test()
		{
			auto socks = LocalAsyncSocket::makePair();
			socks[0].enableLogging();
			socks[1].enableLogging();

			macoro::thread_pool pool;
			auto w = pool.make_work();
			pool.create_thread();

			auto main = std::this_thread::get_id();
			//_ std::cout << "tp " << pool.mState->mThreads[0].get_id() << std::endl;
			//_ std::cout << "mn " << main << std::endl;
			socks[0].setExecutor(pool);
			socks[1].setExecutor(pool);

			auto tt = [&](int s)
			{
				MC_BEGIN(task<>, &, s,
					msg = std::vector<u8>{},
					sock = socks[s].fork());

				msg.resize(10);
				if (s)
				{
					//_ std::cout << "s   " << std::this_thread::get_id() << std::endl;

					MC_AWAIT(sock.send(msg));
					if (std::this_thread::get_id() == main)
						throw COPROTO_RTE;

					//_ std::cout << "sd0 " << std::this_thread::get_id() << std::endl;
					msg.resize(10);
					MC_AWAIT(sock.send(msg));
					if (std::this_thread::get_id() == main)
						throw COPROTO_RTE;

					//_ std::cout << "sd1 " << std::this_thread::get_id() << std::endl;

				}
				else
				{
					//_ std::cout << "r   " << std::this_thread::get_id() << std::endl;

					MC_AWAIT(sock.recv(msg));

					if (std::this_thread::get_id() == main)
						throw COPROTO_RTE;
					//_ std::cout << "rv0 " << std::this_thread::get_id() << std::endl;

					MC_AWAIT(sock.recv(msg));

					if (std::this_thread::get_id() == main)
						throw COPROTO_RTE;

					//_ std::cout << "rv1 " << std::this_thread::get_id() << std::endl;
				}

				MC_END();
			};

			auto r = sync_wait(when_all_ready(tt(0), tt(1)));
			std::get<0>(r).result();
			std::get<1>(r).result();

		}

	}
}
