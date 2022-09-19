#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include <vector>
#include "coproto/Common/Function.h"
#include <string>
#include "coproto/Common/Defines.h"
#include <stdexcept>
	
#include "coproto/Common/CLP.h"

namespace coproto
{
    class TestCollection
    {
    public:
        struct Test
        {
            std::string mName;
            function<void(const CLP&)> mTest;
        };
        TestCollection() = default;
        TestCollection(function<void(TestCollection&)> init)
        {
            init(*this);
        }

        std::vector<Test> mTests;

        enum class Result
        {
            passed,
            skipped,
            failed
        };


        Result runOne(u64 idx, CLP const* cmd = nullptr);
        Result run(std::vector<u64> testIdxs, u64 repeatCount = 1, CLP const* cmd = nullptr);
        Result runAll(uint64_t repeatCount = 1, CLP const* cmd = nullptr);
        Result runIf(CLP& cmd);
        void list();

        std::vector<u64> search(const std::list<std::string>& s);


        void add(std::string name, std::function<void()> test);
        void add(std::string name, std::function<void(const CLP&)> test);

        void operator+=(const TestCollection& add);
    };


    class UnitTestFail : public std::exception
    {
        std::string mWhat;
    public:
        explicit UnitTestFail(std::string reason)
            :std::exception(),
            mWhat(reason)
        {}

        explicit UnitTestFail()
            :std::exception(),
            mWhat("UnitTestFailed exception")
        {
        }

        virtual  const char* what() const throw()
        {
            return mWhat.c_str();
        }
    };

    class UnitTestSkipped : public std::runtime_error
    {
    public:
        UnitTestSkipped()
            : std::runtime_error("skipping test")
        {}

        UnitTestSkipped(std::string r)
            : std::runtime_error(r)
        {}
    };

    enum class Color {
        LightGreen = 2,
        LightGrey = 3,
        LightRed = 4,
        OffWhite1 = 5,
        OffWhite2 = 6,
        Grey = 8,
        Green = 10,
        Blue = 11,
        Red = 12,
        Pink = 13,
        Yellow = 14,
        White = 15,
        Default
    };

    extern const Color ColorDefault;


    std::ostream& operator<<(std::ostream& out, Color color);

	extern TestCollection testCollection;
}
