
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

            try {

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


                auto s0 = std::move(std::get<0>(r).result());
                auto s1 = std::move(std::get<1>(r).result());
                //s0.mSock->mSock.native_handle();
                X509* cert = SSL_get_peer_certificate(s0.mSock->mState->mSock_.native_handle());
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

        struct Wrapper
        {
            using Impl = boost::asio::ip::tcp::socket;
            Impl mImpl;
            std::atomic<bool> mReadFlag, mWriteFlag;

            using lowest_layer_type = Impl::lowest_layer_type;
            using executor_type = Impl::executor_type;

            Wrapper(boost::asio::io_context& ioc)
                : mImpl(ioc)
                , mReadFlag(false)
                , mWriteFlag(false)
            {}

            auto& lowest_layer()
            {
                return mImpl.lowest_layer();
            }


            template <typename MutableBufferSequence>
            std::size_t read_some(const MutableBufferSequence& buffers,
                boost::system::error_code& ec)
            {
                return mImpl.read_some(buffers, ec);
            }


            template <typename ConstBufferSequence>
            std::size_t write_some(const ConstBufferSequence& buffers,
                boost::system::error_code& ec)
            {
                return mImpl.write_some(buffers, ec);
            }


            template <
                typename MutableBufferSequence,
                typename ReadHandler>
                
            auto
                async_read_some(const MutableBufferSequence& buffers,
                    ReadHandler&& handler)
            {
                bool exp = false;
                if (mReadFlag.compare_exchange_strong(exp, true) == false)
                    std::terminate();

                return mImpl.async_read_some(buffers, [this, h = std::forward<ReadHandler>(handler)](boost::system::error_code ec, std::size_t bt) mutable
                {
                    bool exp = true;
                    if (mReadFlag.compare_exchange_strong(exp, false) == false)
                        std::terminate();
                    h(ec, bt);
                });
            }


            template <
                typename ConstBufferSequence,
                typename WriteHandler>

                auto
                async_write_some(const ConstBufferSequence& buffers,
                    WriteHandler&& handler)
            {
                bool exp = false;
                if (mWriteFlag.compare_exchange_strong(exp, true) == false)
                    std::terminate();

                return mImpl.async_write_some(buffers, [this, h = std::forward<WriteHandler>(handler)](boost::system::error_code ec, std::size_t bt)mutable
                {

                    bool exp = true;
                    if (mWriteFlag.compare_exchange_strong(exp, false) == false)
                        std::terminate();
                    h(ec, bt);
                });
            }
        };

        //void minimal()
        //{
        //    using namespace boost::asio;
        //    using namespace boost::asio::ip;

        //    boost::asio::io_context ioc;

        //    auto w = std::make_unique<boost::asio::io_context::work>(ioc);
        //    std::vector<std::thread> thrds(4);
        //    for (auto& t : thrds)
        //        t = std::thread([&] { ioc.run(); });

        //    int port = 1212;
        //    boost::asio::ssl::context serverCtx(boost::asio::ssl::context::tlsv13_server);
        //    boost::asio::ssl::context clientCtx(boost::asio::ssl::context::tlsv13_client);

        //    auto dir = std::string(COPROTO_TEST_DIR) + "/cert";
        //    auto file = dir + "/ca.cert.pem";
        //    serverCtx.load_verify_file(file);
        //    clientCtx.load_verify_file(file);

        //    clientCtx.use_private_key_file(dir + "/client-0.key.pem", boost::asio::ssl::context::file_format::pem);
        //    clientCtx.use_certificate_file(dir + "/client-0.cert.pem", boost::asio::ssl::context::file_format::pem);
        //    serverCtx.use_private_key_file(dir + "/server-0.key.pem", boost::asio::ssl::context::file_format::pem);
        //    serverCtx.use_certificate_file(dir + "/server-0.cert.pem", boost::asio::ssl::context::file_format::pem);

        //    tcp::acceptor acceptor(ioc);
        //    tcp::resolver resolver(ioc);
        //    tcp::endpoint endpoint = *resolver.resolve("localhost", std::to_string(port).c_str());
        //    acceptor.open(endpoint.protocol());
        //    acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        //    acceptor.bind(endpoint);
        //    acceptor.listen(1);

        //    ssl::stream<Wrapper> srv(ioc, serverCtx), cli(ioc, clientCtx);
        //    auto fu = std::async([&] {
        //        acceptor.accept(srv.lowest_layer());
        //        srv.handshake(ssl::stream_base::server);
        //        });
        //    connect(cli.lowest_layer(), &endpoint, &endpoint + 1);
        //    cli.handshake(ssl::stream_base::client);
        //    fu.get();


        //    io_context::strand srvStrand(ioc), cliStrand(ioc);


        //    u64 trials = 100;
        //    //std::vector<char> mData(10000);

        //    std::atomic<u64> c(0);

        //    std::function<void(bool, ssl::stream<Wrapper>&, io_context::strand&, u64)> f =
        //        [&](bool send, ssl::stream<Wrapper>& sock, io_context::strand& strand, u64 t) {
        //        ++c;
        //        strand.dispatch([&, send, t]() {
        //            std::vector<u8> buffer(10000);
        //            if (send)
        //            {
        //                auto bb = const_buffer(buffer.data(), buffer.size());
        //                async_write(sock, bb,
        //                    [&, send, t, buffer = std::move(buffer)](boost::system::error_code error, std::size_t n) mutable {
        //                        if (error)
        //                        {
        //                            std::cout << error.message() << std::endl;
        //                            std::terminate();
        //                        }
        //                        if (t)
        //                            f(send, sock, strand, t - 1);
        //                        --c;
        //                    });
        //            }
        //            else
        //            {
        //                auto bb = mutable_buffer(buffer.data(), buffer.size());
        //                async_read(sock, bb,
        //                    [&, send, t, buffer = std::move(buffer)](boost::system::error_code error, std::size_t n) mutable {
        //                        //callback(error, n/*, s*/);
        //                        if (error)
        //                        {
        //                            std::cout << error.message() << std::endl;
        //                            std::terminate();
        //                        }

        //                        if (t)
        //                            f(send, sock, strand, t - 1);
        //                        --c;
        //                    }
        //                );
        //            }
        //            });

        //    };

        //    f(true, srv, srvStrand, trials);
        //    f(false, srv, srvStrand, trials);
        //    f(true, cli, cliStrand, trials);
        //    f(false, cli, cliStrand, trials);

        //    while (c);
        //    w.reset();
        //    for (auto& t : thrds)
        //        t.join();

        //}

        void AsioTlsSocket_sendRecv_base_test()
        {
            u64 trials = 1000;
            boost::asio::io_context ioc;
            optional<boost::asio::io_context::work> w(ioc);
            std::vector<std::thread> thrds(4);
            for (auto& t : thrds)
                t = std::thread([&] { ioc.run(); });

            boost::asio::ssl::context serverCtx(boost::asio::ssl::context::tlsv13_server);
            boost::asio::ssl::context clientCtx(boost::asio::ssl::context::tlsv13_client);
            //serverCtx.set_verify_mode(
            //    boost::asio::ssl::verify_peer |
            //    boost::asio::ssl::verify_fail_if_no_peer_cert);
            //serverCtx.set_verify_mode(
            //    boost::asio::ssl::verify_peer |
            //    boost::asio::ssl::verify_fail_if_no_peer_cert);

            auto dir = std::string(COPROTO_TEST_DIR) + "/cert";
            auto file = dir + "/ca.cert.pem";
            //serverCtx.load_verify_file(file);
            clientCtx.load_verify_file(file);
            //clientCtx.use_private_key_file(dir + "/client-0.key.pem", boost::asio::ssl::context::file_format::pem);
            //clientCtx.use_certificate_file(dir + "/client-0.cert.pem", boost::asio::ssl::context::file_format::pem);
            serverCtx.use_private_key_file(dir + "/server-0.key.pem", boost::asio::ssl::context::file_format::pem);
            serverCtx.use_certificate_file(dir + "/server-0.cert.pem", boost::asio::ssl::context::file_format::pem);
            auto address = "localhost:1212";

            auto r = macoro::sync_wait(macoro::when_all_ready(
                macoro::make_task(AsioTlsAcceptor(address, ioc, serverCtx)),
                macoro::make_task(AsioTlsConnect(address, ioc, clientCtx))
            ));

            //auto r = macoro::sync_wait(macoro::when_all_ready(
            //    macoro::make_task(AsioAcceptor(address, ioc)),
            //    macoro::make_task(AsioConnect(address, ioc))
            //));
            try {

                auto srv = std::get<0>(r).result();
                auto cli = std::get<1>(r).result();
                std::vector<char> mData(10000);

                std::atomic<u64> c(0);
                std::vector<std::shared_future<void>> fut;

                std::function<void(bool, decltype(*srv.mSock->mState)&, u64)> f =
                    [&](bool send, decltype(*srv.mSock->mState)& state, u64 t) {
                    ++c;
                    using namespace boost::asio;
                    //fut[t].get();
                    if (send)
                    {

                        async_write(state.mSock_, boost::asio::const_buffer(mData.data(), mData.size()),
                            [&, send, t](boost::system::error_code error, std::size_t n) mutable {
                                //callback(error, n/*, s*/);
                                if (error)
                                {
                                    std::cout << error.message() << std::endl;
                                    std::terminate();
                                }
                                if (t)
                                    f(send, state, t - 1);
                                --c;
                                //std::cout << send << t << " " << c << " " << send << " " << (u64)&state << std::endl;
                            });
                    }
                    else
                    {
                        async_read(state.mSock_, boost::asio::mutable_buffer(mData.data(), mData.size()),
                            [&, send, t](boost::system::error_code error, std::size_t n) mutable {
                                //callback(error, n/*, s*/);
                                if (error)
                                {
                                    std::cout << error.message() << std::endl;
                                    std::terminate();
                                }

                                if (t)
                                    f(send, state, t - 1);
                                --c;
                                //std::cout << send << t << " " << c << " " << send << " " << (u64)&state << std::endl;
                            }
                        );
                    }

                };

                boost::asio::post(srv.mSock->mState->mSock_.get_executor(), [&]() { f(true, *srv.mSock->mState, trials); });
                boost::asio::post(srv.mSock->mState->mSock_.get_executor(), [&]() {f(false, *srv.mSock->mState, trials); });
                boost::asio::post(cli.mSock->mState->mSock_.get_executor(), [&]() {f(true, *cli.mSock->mState, trials); });
                boost::asio::post(cli.mSock->mState->mSock_.get_executor(), [&]() {f(false, *cli.mSock->mState, trials); });

                while (c);
                w.reset();
                for (auto& t : thrds)
                    t.join();
            }
            catch (std::exception& e)
            {
                std::cout << e.what() << std::endl;
                throw;
            }
        }


        void AsioTlsSocket_sendRecv_test()
        {

            //u64 trials = 100;
            u64 numOps = 200;

            boost::asio::io_context ioc;
            optional<boost::asio::io_context::work> w(ioc);
            std::vector<std::thread> thrds(4);
            for (auto& t : thrds)
                t = std::thread([&] { ioc.run(); });

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
            clientCtx.use_private_key_file(dir + "/client-0.key.pem", boost::asio::ssl::context::file_format::pem);
            clientCtx.use_certificate_file(dir + "/client-0.cert.pem", boost::asio::ssl::context::file_format::pem);
            serverCtx.use_private_key_file(dir + "/server-0.key.pem", boost::asio::ssl::context::file_format::pem);
            serverCtx.use_certificate_file(dir + "/server-0.cert.pem", boost::asio::ssl::context::file_format::pem);

            auto address = "localhost:1212";
            auto S = macoro::sync_wait(macoro::when_all_ready(
                macoro::make_task(AsioTlsAcceptor(address, ioc, serverCtx)),
                macoro::make_task(AsioTlsConnect(address, ioc, clientCtx))
            ));


            auto task_ = [&](bool sender, AsioTlsSocket& ss) -> task<void> {

                MC_BEGIN(task<>, &, sender, ss/* = AsioTlsSocket{}*/, i = u64{},
                    res = macoro::result<void>{},
                    sb = std::vector<u8>{},
                    rb = std::vector<u8>(10000)
                );
                //if (sender)
                //    MC_AWAIT_SET(ss, AsioTlsAcceptor("localhost:1212", ioc, serverCtx));
                //else
                //    MC_AWAIT_SET(ss, AsioTlsConnect("localhost:1212", ioc, clientCtx));


                //std::this_thread::sleep_for(std::chrono::milliseconds(10));

                for (i = 0; i < numOps; ++i)
                {
                    sb.resize(10000);
                    MC_AWAIT(macoro::when_all_ready(
                        ss.mSock->send(sb),
                        ss.mSock->recv(rb)
                    ));
                    //MC_AWAIT_TRY(res, ss.send(std::move(sb)));
                    ////MC_AWAIT_TRY(res, ss.mSock->send(std::move(sb)));

                    //if (res.has_error())
                    //{
                    //    try { std::rethrow_exception(res.error()); }
                    //    catch (std::exception& e)
                    //    {
                    //        std::cout << e.what() << std::endl;
                    //        throw;
                    //    }
                    //}
                    //MC_AWAIT_TRY(res, ss.recv(rb));
                    ////MC_AWAIT_TRY(res, ss.mSock->recv(rb));

                    //if (res.has_error())
                    //{
                    //    try { std::rethrow_exception(res.error()); }
                    //    catch (std::exception& e)
                    //    {
                    //        std::cout << e.what() << std::endl;
                    //        throw;
                    //    }
                    //}
                }

                MC_AWAIT(ss.flush());

                MC_END();
            };

            auto e0 = task_(0, std::get<0>(S).result());
            auto e1 = task_(1, std::get<1>(S).result());

            auto r = macoro::sync_wait(macoro::when_all_ready(
                std::move(e0), std::move(e1)
            ));
            std::get<0>(r).result();
            std::get<1>(r).result();

            w.reset();
            for (auto& t : thrds)
                t.join();
        }

        void AsioTlsSocket_parSendRecv_test()
        {

            u64 trials = 100;
            u64 numOps = 200;
            boost::asio::io_context ioc;
            optional<boost::asio::io_context::work> w(ioc);

            std::vector<std::thread> thrds(4);
            for (auto& t : thrds)
                t = std::thread([&] {ioc.run(); });

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


            clientCtx.use_private_key_file(dir + "/client-0.key.pem", boost::asio::ssl::context::file_format::pem);
            clientCtx.use_certificate_file(dir + "/client-0.cert.pem", boost::asio::ssl::context::file_format::pem);
            serverCtx.use_private_key_file(dir + "/server-0.key.pem", boost::asio::ssl::context::file_format::pem);
            serverCtx.use_certificate_file(dir + "/server-0.cert.pem", boost::asio::ssl::context::file_format::pem);

            //ctx.add_certificate_authority()
            using namespace boost::asio;

            for (u64 tt = 0; tt < trials; ++tt)
            {

                auto r = macoro::sync_wait(macoro::when_all_ready(
                    macoro::make_task(AsioTlsAcceptor(address, ioc, serverCtx)),
                    macoro::make_task(AsioTlsConnect(address, ioc, clientCtx))
                ));


                std::array<AsioTlsSocket, 2> s
                { {
                    std::get<0>(r).result(),
                    std::get<1>(r).result()
                } };

                auto f1 = [&](u64 idx) {
                    MC_BEGIN(task<void>, idx, &numOps, address, &ioc, &serverCtx, &clientCtx, &s,
                        v = u64{},
                        i = u64{},
                        buffer = span<u8>{},
                        r = std::pair<error_code, u64>{});

                    buffer = span<u8>((u8*)&v, sizeof(v));



                    //MC_AWAIT(macoro::transfer_to(ex[idx]));

                    for (i = 1; i <= numOps; ++i)
                    {

                        if (idx == 0) {
                            MC_AWAIT_SET(r, s[0].mSock->recv(buffer));
                            COPROTO_ASSERT(v == -i);
                        }
                        if (idx == 1)
                        {
                            MC_AWAIT_SET(r, s[1].mSock->recv(buffer));
                            COPROTO_ASSERT(v == i);
                        }
                        if (idx == 2)
                        {
                            v = i;
                            MC_AWAIT_SET(r, s[0].mSock->send(buffer));
                        }
                        if (idx == 3)
                        {
                            v = -i;
                            MC_AWAIT_SET(r, s[1].mSock->send(buffer));
                        }

                        COPROTO_ASSERT(!r.first);
                        COPROTO_ASSERT(r.second == sizeof(v));

                        //MC_AWAIT(macoro::transfer_to(ex[idx]));
                    }
                    MC_END();
                };

                macoro::sync_wait(macoro::when_all_ready(f1(0), f1(1), f1(2), f1(3)));
            }

            w.reset();
            for (auto& t : thrds)
                t.join();
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

