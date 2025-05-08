#include "stdafx.h"
#include "CppUnitTest.h"

#include "util/StrToken.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

constexpr const char TEST_STR[MAX_CHARS_IN_STRTOKEN + 1] = "abcde_ghijkz";
constexpr const char TEST_STR_INDEX[MAX_CHARS_IN_STRTOKEN + 1] = "abcde_ghijkz9";
constexpr const char TEST_STR_SMALL[MAX_CHARS_IN_STRTOKEN + 1] = "dog";

constexpr StrToken TOKEN_MAX_SIZE(TEST_STR);
constexpr StrToken TOKEN_MAX_SIZE_INDEX1(TEST_STR, 12);
constexpr StrToken TOKEN_MAX_SIZE_INDEX2(TEST_STR_INDEX);
constexpr StrToken TOKEN_SMALL(TEST_STR_SMALL, 6);

// To run these tests, Test->Test Explorer
namespace UNITTESTS
{
	TEST_CLASS(SIMPLE)
	{
	public:
		TEST_METHOD(StrTokenTest)
        {
			char buf[255];
			ui32 len;

            TOKEN_MAX_SIZE.toString(buf, &len);
            Logger::WriteMessage(buf);
            Logger::WriteMessage("\n");
            Assert::IsTrue(strcmp(buf, TEST_STR) == 0 && len == MAX_CHARS_IN_STRTOKEN, L"TOKEN_MAX_SIZE was not converted properly to string");

            TOKEN_MAX_SIZE_INDEX1.toString(buf, &len);
            Logger::WriteMessage(buf);
            Logger::WriteMessage("\n");
            Assert::IsTrue(strcmp(buf, "abcde_ghijkz12") == 0 && len == MAX_CHARS_IN_STRTOKEN + 1, L"TOKEN_MAX_SIZE_INDEX1 was not converted properly to string");

            TOKEN_MAX_SIZE_INDEX2.toString(buf, &len);
            Logger::WriteMessage(buf);
            Logger::WriteMessage("\n");
            Assert::IsTrue(strcmp(buf, "abcde_ghijkz09") == 0 && len == MAX_CHARS_IN_STRTOKEN + 1, L"TOKEN_MAX_SIZE_INDEX2 was not converted properly to string");

            TOKEN_SMALL.toString(buf, &len);
            Logger::WriteMessage(buf);
            Logger::WriteMessage("\n");
            Assert::IsTrue(strcmp(buf, "dog06") == 0 && len == 5, L"TOKEN_SMALL was not converted properly to string");
		}
	};
}
