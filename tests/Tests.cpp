
#include "Tests.h"
#include "coproto/coproto.h"
#include "tests/TaskProto_tests.h"
#include "tests/TaskProto14_tests.h"
#include "tests/LocalAsyncSocket_tests.h"
#include "tests/SocketScheduler_tests.h"
#include "tests/BufferingSocket_tests.h"
#include "tests/AsioSocket_tests.h"
#include "tests/AsioTlsSocket_tests.h"

#ifdef _MSC_VER
#include <windows.h>
#endif
#include <iomanip>
#include <chrono>
#include <cmath>

namespace coproto
{


    const Color ColorDefault([]() -> Color {
#ifdef _MSC_VER
        CONSOLE_SCREEN_BUFFER_INFO   csbi;
        HANDLE m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleScreenBufferInfo(m_hConsole, &csbi);

        return (Color)(csbi.wAttributes & 255);
#else
        return Color::White;
#endif

        }());

#ifdef _MSC_VER
    static const HANDLE __m_hConsole(GetStdHandle(STD_OUTPUT_HANDLE));
#endif
#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

    std::array<const char*, 16> colorMap
    {
        "",         //    -- = 0,
        "",         //    -- = 1,
        GREEN,      //    LightGreen = 2,
        BLACK,      //    LightGrey = 3,
        RED,        //    LightRed = 4,
        WHITE,      //    OffWhite1 = 5,
        WHITE,      //    OffWhite2 = 6,
        "",         //         = 7
        BLACK,      //    Grey = 8,
        "",         //    -- = 9,
        BOLDGREEN,  //    Green = 10,
        BOLDBLUE,   //    Blue = 11,
        BOLDRED,    //    Red = 12,
        BOLDCYAN,   //    Pink = 13,
        BOLDYELLOW, //    Yellow = 14,
        RESET       //    White = 15
    };

    std::ostream& operator<<(std::ostream& out, Color tag)
    {
        if (tag == Color::Default)
            tag = ColorDefault;
#ifdef _MSC_VER
        SetConsoleTextAttribute(__m_hConsole, (WORD)tag | (240 & (WORD)ColorDefault));
#else

        out << colorMap[15 & (char)tag];
#endif
        return out;
    }


    void TestCollection::add(std::string name, std::function<void(const CLP&)> fn)
    {
        mTests.push_back({ std::move(name), std::move(fn) });
    }
    void TestCollection::add(std::string name, std::function<void()> fn)
    {
        mTests.push_back({ std::move(name),[fn](const CLP& cmd)
        {
            fn();
        } });
    }

    TestCollection::Result TestCollection::runOne(uint64_t idx, CLP const* cmd)
    {
        if (idx >= mTests.size())
        {
            std::cout << Color::Red << "No test " << idx << std::endl;
            return Result::failed;
        }

        CLP dummy;
        if (cmd == nullptr)
            cmd = &dummy;

        Result res = Result::failed;
        int w = int(std::ceil(std::log10(mTests.size())));
        std::cout << std::setw(w) << idx << " - " << Color::Blue << mTests[idx].mName << ColorDefault << std::flush;

        auto start = std::chrono::high_resolution_clock::now();
        try
        {
            mTests[idx].mTest(*cmd); std::cout << Color::Green << "  Passed" << ColorDefault;
            res = Result::passed;
        }
        catch (const UnitTestSkipped& e)
        {
            std::cout << Color::Yellow << "  Skipped - " << e.what() << ColorDefault;
            res = Result::skipped;
        }
        catch (const std::exception& e)
        {
            std::cout << Color::Red << "Failed - " << e.what() << ColorDefault;
        }
        auto end = std::chrono::high_resolution_clock::now();



        uint64_t time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "   " << time << "ms" << std::endl;

        return res;
        }

    TestCollection::Result TestCollection::run(std::vector<u64> testIdxs, u64 repeatCount, CLP const* cmd)
    {
        u64 numPassed(0), total(0), numSkipped(0);

        for (u64 r = 0; r < repeatCount; ++r)
        {
            for (auto i : testIdxs)
            {
                if (repeatCount != 1) std::cout << r << " ";
                auto res = runOne(i, cmd);
                numPassed += (res == Result::passed);
                total += (res != Result::skipped);
                numSkipped += (res == Result::skipped);
            }
        }

        if (numPassed == total)
        {
            std::cout << Color::Green << std::endl
                << "=============================================\n"
                << "            All Passed (" << numPassed << ")\n";
            if (numSkipped)
                std::cout << Color::Yellow << "            skipped (" << numSkipped << ")\n";

            std::cout << Color::Green
                << "=============================================" << std::endl << ColorDefault;
            return Result::passed;
        }
        else
        {
            std::cout << Color::Red << std::endl
                << "#############################################\n"
                << "           Failed (" << total - numPassed << ")\n" << Color::Green
                << "           Passed (" << numPassed << ")\n";

            if (numSkipped)
                std::cout << Color::Yellow << "            skipped (" << numSkipped << ")\n";

            std::cout << Color::Red
                << "#############################################" << std::endl << ColorDefault;
            return Result::failed;
        }
    }

    std::vector<u64> TestCollection::search(const std::list<std::string>& s)
    {
        std::set<u64> ss;
        std::vector<u64> ret;
        std::vector<std::string> names;

        auto toLower = [](std::string data) {
            std::transform(data.begin(), data.end(), data.begin(),
                [](unsigned char c) { return std::tolower(c); });
            return data;
        };

        for (auto& t : mTests)
            names.push_back(toLower(t.mName));

        for (auto str : s)
        {
            auto lStr = toLower(str);
            for (auto& t : names)
            {
                if (t.find(lStr) != std::string::npos)
                {
                    auto i = &t - names.data();
                    if (ss.insert(i).second)
                        ret.push_back(i);
                }
            }
        }

        return ret;
    }

    TestCollection::Result TestCollection::runIf(CLP& cmd)
    {
        if (cmd.isSet("list"))
        {
            list();
            return Result::passed;
        }
        auto unitTestTag = std::vector<std::string>{ "u","unitTests" };
        if (cmd.isSet(unitTestTag))
        {
            cmd.setDefault("loop", 1);
            auto loop = cmd.get<u64>("loop");

            if (cmd.hasValue(unitTestTag))
            {
                auto& str = cmd.getList(unitTestTag);
                if (str.front().size() && std::isalpha(str.front()[0]))
                    return run(search(str), loop, &cmd);
                else
                    return run(cmd.getMany<u64>(unitTestTag), loop, &cmd);
            }
            else
                return runAll(loop, &cmd);
        }
        return Result::skipped;
    }

    TestCollection::Result TestCollection::runAll(uint64_t rp, CLP const* cmd)
    {
        std::vector<u64> v;
        for (u64 i = 0; i < mTests.size(); ++i)
            v.push_back(i);

        return run(v, rp, cmd);
    }

    void TestCollection::list()
    {
        int w = int(std::ceil(std::log10(mTests.size())));
        for (uint64_t i = 0; i < mTests.size(); ++i)
        {
            std::cout << std::setw(w) << i << " - " << Color::Blue << mTests[i].mName << std::endl << ColorDefault;
        }
    }


    void TestCollection::operator+=(const TestCollection& t)
    {
        mTests.insert(mTests.end(), t.mTests.begin(), t.mTests.end());
    }



    TestCollection testCollection([](TestCollection& t) {

        t.add("InlinePolyTest                        ", tests::InlinePolyTest);
        
        t.add("LocalAsyncSocket_sendRecv_test        ", tests::LocalAsyncSocket_sendRecv_test);
        t.add("LocalAsyncSocket_parSendRecv_test     ", tests::LocalAsyncSocket_parSendRecv_test);
        t.add("LocalAsyncSocket_cancellation_test    ", tests::LocalAsyncSocket_cancellation_test);
        t.add("LocalAsyncSocket_close_test           ", tests::LocalAsyncSocket_close_test);

        t.add("BufferingSocket_sendRecv_test         ", tests::BufferingSocket_sendRecv_test);
        t.add("BufferingSocket_asyncSend_test        ", tests::BufferingSocket_asyncSend_test);
        t.add("BufferingSocket_parSendRecv_test      ", tests::BufferingSocket_parSendRecv_test);
        t.add("BufferingSocket_cancellation_test     ", tests::BufferingSocket_cancellation_test);
        t.add("BufferingSocket_parCancellation_test  ", tests::BufferingSocket_parCancellation_test);
        t.add("BufferingSocket_close_test            ", tests::BufferingSocket_close_test);
        

        t.add("AsioSocket_Accept_test                ", tests::AsioSocket_Accept_test);
        t.add("AsioSocket_Accept_sCancel_test        ", tests::AsioSocket_Accept_sCancel_test);
        t.add("AsioSocket_Accept_cCancel_test        ", tests::AsioSocket_Accept_cCancel_test);
        t.add("AsioSocket_sendRecv_test              ", tests::AsioSocket_sendRecv_test);
        t.add("AsioSocket_asyncSend_test             ", tests::AsioSocket_asyncSend_test);
        t.add("AsioSocket_parSendRecv_test           ", tests::AsioSocket_parSendRecv_test);
        t.add("AsioSocket_cancellation_test          ", tests::AsioSocket_cancellation_test);
        t.add("AsioSocket_parCancellation_test       ", tests::AsioSocket_parCancellation_test);
        t.add("AsioSocket_close_test                 ", tests::AsioSocket_close_test);

        t.add("AsioTlsSocket_Accept_test             ", tests::AsioTlsSocket_Accept_test);
        t.add("AsioTlsSocket_Accept_sCacnel_test     ", tests::AsioTlsSocket_Accept_sCacnel_test);
        t.add("AsioTlsSocket_Accept_cCacnel_test     ", tests::AsioTlsSocket_Accept_cCacnel_test);
        
        t.add("AsioTlsSocket_sendRecv_base_test      ", tests::AsioTlsSocket_sendRecv_base_test);
        t.add("AsioTlsSocket_sendRecv_test           ", tests::AsioTlsSocket_sendRecv_test);
        t.add("AsioTlsSocket_parSendRecv_test        ", tests::AsioTlsSocket_parSendRecv_test);
        

        t.add("SocketScheduler_basicSend_test        ", tests::SocketScheduler_basicSend_test);
        t.add("SocketScheduler_basicRecv_test        ", tests::SocketScheduler_basicRecv_test);
        t.add("SocketScheduler_cancelSend_test       ", tests::SocketScheduler_cancelSend_test);
        t.add("SocketScheduler_cancelRecv_test       ", tests::SocketScheduler_cancelRecv_test);
        t.add("SocketScheduler_restoreSend_test      ", tests::SocketScheduler_restoreSend_test);
        t.add("SocketScheduler_restoreRecv_test      ", tests::SocketScheduler_restoreRecv_test);
        t.add("SocketScheduler_closeSend_test        ", tests::SocketScheduler_closeSend_test);
        t.add("SocketScheduler_closeRecv_test        ", tests::SocketScheduler_closeRecv_test);
        t.add("SocketScheduler_pendingRecvClose_test ", tests::SocketScheduler_pendingRecvClose_test);
        t.add("SocketScheduler_badFirstMgs_test      ", tests::SocketScheduler_badFirstMgs_test);
        t.add("SocketScheduler_repeatInitSlot_test   ", tests::SocketScheduler_repeatInitSlot_test);
        t.add("SocketScheduler_badSlotSend_test      ", tests::SocketScheduler_badSlotSend_test);
        t.add("SocketScheduler_executor_test         ", tests::SocketScheduler_executor_test);
        
        t.add("task_proto_test                       ", tests::task_proto_test);
        t.add("task_strSendRecv_Test                 ", tests::task_strSendRecv_Test);
        t.add("task_resultSendRecv_Test              ", tests::task_resultSendRecv_Test);
        t.add("task_typedRecv_Test                   ", tests::task_typedRecv_Test);
        t.add("task_zeroSendRecv_Test                ", tests::task_zeroSendRecv_Test);
        t.add("task_zeroSendRecv_ErrorCode_Test      ", tests::task_zeroSendRecv_ErrorCode_Test);
        t.add("task_badRecvSize_Test                 ", tests::task_badRecvSize_Test);
        t.add("task_badRecvSize_ErrorCode_Test       ", tests::task_badRecvSize_ErrorCode_Test);
        t.add("task_moveSend_Test                    ", tests::task_moveSend_Test);
        t.add("task_throws_Test                      ", tests::task_throws_Test);
        t.add("task_nestedProtocol_Test              ", tests::task_nestedProtocol_Test);
        t.add("task_nestedProtocol_Throw_Test        ", tests::task_nestedProtocol_Throw_Test);
        t.add("task_nestedProtocol_ErrorCode_Test    ", tests::task_nestedProtocol_ErrorCode_Test);
        t.add("task_asyncProtocol_Test               ", tests::task_asyncProtocol_Test);
        t.add("task_asyncProtocol_Throw_Test         ", tests::task_asyncProtocol_Throw_Test);
        t.add("task_endOfRound_Test                  ", tests::task_endOfRound_Test);
        t.add("task_errorSocket_Test                 ", tests::task_errorSocket_Test);
        t.add("task_cancel_send_test                 ", tests::task_cancel_send_test);
        t.add("task_cancel_recv_test                 ", tests::task_cancel_recv_test);

        t.add("task14_proto_test                     ", tests::task14_proto_test);
        t.add("task14_strSendRecv_Test               ", tests::task14_strSendRecv_Test);
        t.add("task14_resultSendRecv_Test            ", tests::task14_resultSendRecv_Test);
        t.add("task14_typedRecv_Test                 ", tests::task14_typedRecv_Test);
        t.add("task14_zeroSendRecv_Test              ", tests::task14_zeroSendRecv_Test);
        t.add("task14_zeroSendRecv_ErrorCode_Test    ", tests::task14_zeroSendRecv_ErrorCode_Test);
        t.add("task14_badRecvSize_Test               ", tests::task14_badRecvSize_Test);
        t.add("task14_badRecvSize_ErrorCode_Test     ", tests::task14_badRecvSize_ErrorCode_Test);
        t.add("task14_moveSend_Test                  ", tests::task14_moveSend_Test);
        t.add("task14_throws_Test                    ", tests::task14_throws_Test);
        t.add("task14_nestedProtocol_Test            ", tests::task14_nestedProtocol_Test);
        t.add("task14_nestedProtocol_Throw_Test      ", tests::task14_nestedProtocol_Throw_Test);
        t.add("task14_nestedProtocol_ErrorCode_Test  ", tests::task14_nestedProtocol_ErrorCode_Test);
        t.add("task14_asyncProtocol_Test             ", tests::task14_asyncProtocol_Test);
        t.add("task14_asyncProtocol_Throw_Test       ", tests::task14_asyncProtocol_Throw_Test);
        t.add("task14_endOfRound_Test                ", tests::task14_endOfRound_Test);
        t.add("task14_errorSocket_Test               ", tests::task14_errorSocket_Test);
        t.add("task14_cancel_send_test               ", tests::task14_cancel_send_test);
        t.add("task14_cancel_recv_test               ", tests::task14_cancel_recv_test);
        t.add("task14_timeout_test                   ", tests::task14_timeout_test);

        
        return;

        });
}