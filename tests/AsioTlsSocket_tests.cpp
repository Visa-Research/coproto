
#include "coproto/Socket/AsioSocket.h"
#include "tests/config.h"
#include "AsioTlsSocket_tests.h"
#include "Tests.h"
#include <thread>

namespace coproto
{
	namespace tests
	{

#if defined(COPROTO_ENABLE_BOOST) && defined(COPROTO_ENABLE_OPENSSL)

		void AsioTlsSocket_Accept_test()
		{
			boost::asio::io_context ioc;
			optional<boost::asio::io_context::work> w(ioc);
			auto f = std::async([&] { ioc.run(); });

			std::string address("localhost:1212");
			boost::asio::ssl::context serverCtx(boost::asio::ssl::context::tlsv13_server);
			boost::asio::ssl::context clientCtx(boost::asio::ssl::context::tlsv13_client);
			serverCtx.set_verify_mode(
				boost::asio::ssl::verify_peer |
				boost::asio::ssl::verify_fail_if_no_peer_cert);
			serverCtx.set_verify_mode(
				boost::asio::ssl::verify_peer |
				boost::asio::ssl::verify_fail_if_no_peer_cert);

			auto dir = std::string(COPROTO_TEST_DIR) + "/cert";
			auto file = dir + "/ca.cert.pem";
			serverCtx.load_verify_file(file);
			clientCtx.load_verify_file(file);

			serverCtx.set_verify_callback([&](bool verifided, boost::asio::ssl::verify_context& ctx)
				{
					return verifided;
				});

			clientCtx.set_verify_callback([&](bool verifided, boost::asio::ssl::verify_context& ctx)
				{
					X509_STORE_CTX* c = ctx.native_handle();
					return verifided;
				});
			clientCtx.use_private_key_file(dir + "/client-0.key.pem", boost::asio::ssl::context::file_format::pem);
			clientCtx.use_certificate_file(dir + "/client-0.cert.pem", boost::asio::ssl::context::file_format::pem);
			serverCtx.use_private_key_file(dir + "/server-0.key.pem", boost::asio::ssl::context::file_format::pem);
			serverCtx.use_certificate_file(dir + "/server-0.cert.pem", boost::asio::ssl::context::file_format::pem);

			//ctx.add_certificate_authority()
			using namespace boost::asio;
			auto r = macoro::sync_wait(macoro::when_all_ready(
				macoro::make_task(AsioTlsAcceptor(address, ioc, serverCtx)),
				macoro::make_task(AsioTlsConnect(address, ioc, clientCtx))
			));

			try {

				auto s0 = std::move(std::get<0>(r).result());
				auto s1 = std::move(std::get<1>(r).result());
				//s0.mSock->mSock.native_handle();
				X509* cert = SSL_get_peer_certificate(s0.mSock->mState->mSock.native_handle());
				X509_NAME* name = X509_get_subject_name(cert);
				std::string str(2048, (char)0);
				X509_NAME_oneline(name, &str[0], str.size());
				str.resize(strlen(str.c_str()));

				OpenSslX509 cx509 = getX509(s0);
				auto sx509 = getX509(s1);
				if (str != cx509.oneline())
					throw MACORO_RTE_LOC;
				auto serverX509 = getX509(serverCtx);
				auto clientX509 = getX509(clientCtx);

				auto sx509_oneline = sx509.oneline();
				auto cx509_oneline = cx509.oneline();
				auto serverx509_oneline = serverX509.oneline();
				auto clientx509_oneline = clientX509.oneline();

				if (cx509.oneline() != clientX509.oneline())
					throw MACORO_RTE_LOC;
				if (sx509.oneline() != serverX509.oneline())
					throw MACORO_RTE_LOC;

				if (sx509.commonName() != "server-0")
				{
					std::cout << sx509.commonName() << std::endl;
					throw MACORO_RTE_LOC;
				}
				if (cx509.commonName() != "client-0")
				{
					std::cout << cx509.commonName() << std::endl;
					throw MACORO_RTE_LOC;
				}

				//clientCtx.native_handle()
				//std::cout << str << std::endl;
			}
			catch (std::exception& e)
			{
				std::cout << e.what() << std::endl;
				throw;
			}
			std::vector<u8> buff(10);

			w.reset();
			f.get();
		}

		void AsioTlsSocket_Accept_sCacnel_test()
		{

			for (u64 tt = 0; tt < 2; ++tt)
			{

				boost::asio::io_context ioc;
				optional<boost::asio::io_context::work> w(ioc);
				auto f = std::async([&] { ioc.run(); });
				std::string address("localhost:1212");

				boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv13_server);
				ctx.set_verify_mode(
					boost::asio::ssl::verify_peer |
					boost::asio::ssl::verify_fail_if_no_peer_cert);

				AsioTlsAcceptor a(address, ioc, ctx);

				macoro::stop_source src;
				auto token = src.get_token();
				std::thread thrd([&] {
					std::this_thread::sleep_for(std::chrono::milliseconds(20));
					src.request_stop();
					});
				try {

					try {
						if (tt)
						{
							auto s = macoro::sync_wait(macoro::when_all_ready(
								macoro::make_task(a.accept(token)),
								macoro::make_task(AsioConnect(address, ioc))
							));


							std::get<0>(s).result();
							std::get<1>(s).result();
						}
						else
						{
							auto s = macoro::sync_wait(macoro::make_task(a.accept(token)));

						}
						throw MACORO_RTE_LOC;
					}
					catch (std::system_error e)
					{
					}
				}
				catch (...)
				{
					w.reset();
					f.get();
					thrd.join();
					throw;
				}


				w.reset();
				f.get();
				thrd.join();

			}

		}

		void AsioTlsSocket_Accept_cCacnel_test()
		{

			for (u64 tt = 0; tt < 2; ++tt)
			{

				boost::asio::io_context ioc;
				optional<boost::asio::io_context::work> w(ioc);
				auto f = std::async([&] { ioc.run(); });
				std::string address("localhost:1212");

				boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv13_client);
				ctx.set_verify_mode(
					boost::asio::ssl::verify_peer |
					boost::asio::ssl::verify_fail_if_no_peer_cert);


				macoro::stop_source src;
				auto token = src.get_token();
				std::thread thrd([&] {
					std::this_thread::sleep_for(std::chrono::milliseconds(20));
					src.request_stop();
					});
				try {

					try {
						if (tt)
						{
							auto s = macoro::sync_wait(macoro::when_all_ready(
								macoro::make_task(AsioTlsConnect(address, ioc, ctx, token)),
								macoro::make_task(AsioAcceptor(address, ioc))
							));


							std::get<0>(s).result();
							std::get<1>(s).result();
						}
						else
						{
							auto s = macoro::sync_wait(macoro::make_task(AsioTlsConnect(address, ioc, ctx, token)));

						}
						throw MACORO_RTE_LOC;
					}
					catch (std::system_error e)
					{
					}
				}
				catch (...)
				{
					w.reset();
					f.get();
					thrd.join();
					throw;
				}


				w.reset();
				f.get();
				thrd.join();

			}

		}

#else

		namespace
		{
			void skip() { throw UnitTestSkipped("Boost and/or openssl not enabled"); }
		}

		void AsioTlsSocket_Accept_test() { skip(); }
		void AsioTlsSocket_Accept_sCacnel_test() { skip(); }
		void AsioTlsSocket_Accept_cCacnel_test() { skip(); }


#endif

	}
}

