#include <gtest/gtest.h>
#include "Piwik.h"

TEST(Encoding, PercentEncodingPreservesUnreservedCharacters)
{
    std::string input = "AZaz09-_.~";
    EXPECT_EQ(input, PercentEncode(input));
}

TEST(Encoding, PercentEncodingEscapesQueryDelimitersAndSpaces)
{
    std::string input = "a b&c=1?/#%+";
    EXPECT_EQ("a%20b%26c%3D1%3F%2F%23%25%2B", PercentEncode(input));
}

TEST(Encoding, JsonEncodingEscapesControlCharacters)
{
    std::string input = "\"\\\n\r\t";
    EXPECT_EQ("\\\"\\\\\\n\\r\\t", JsonEncode(input));
}

TEST(Encoding, UnicodeRoundTripsThroughUtf8)
{
    std::wstring original = L"\u05e9\u05dc\u05d5\u05dd \u00e9 \U0001f680";
    auto encoded = ToUTF8(original);
    EXPECT_EQ(original, ToWide(encoded));
}

TEST(Encoding, EmptyStringsRemainEmpty)
{
    std::string narrow;
    std::wstring wide;
    EXPECT_TRUE(PercentEncode(narrow).empty());
    EXPECT_TRUE(JsonEncode(narrow).empty());
    EXPECT_TRUE(ToUTF8(wide).empty());
    EXPECT_TRUE(ToWide(narrow).empty());
}
