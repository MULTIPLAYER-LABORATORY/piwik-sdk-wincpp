#include <gtest/gtest.h>
#include <limits>
#include <locale>
#include "Piwik.h"

namespace {
PiwikState Measurement()
{
    PiwikState state;
    state.TrackedPath = _T("/startup");
    state.EventCategory = _T("Performance");
    state.EventAction = _T("Startup");
    state.EventName = _T("ui_ready_ms");
    state.SetEventValue(0.0);
    return state;
}
size_t Count(const std::string& text, const std::string& needle)
{
    size_t count = 0, pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos)
    {
        ++count;
        pos += needle.size();
    }
    return count;
}
class Dimensions : public ::testing::TestWithParam<PiwikQueryFormat> {};
}

TEST(ActionDimensions, ValidatesIdsWithoutChangingTheCollection)
{
    PiwikActionDimensions dimensions;
    ASSERT_TRUE(dimensions.Set(1, _T("failure")));
    ASSERT_TRUE(dimensions.Set(999, _T("Win32")));
    for (int id : {-1, 0, 1000, (std::numeric_limits<int>::max)()})
        EXPECT_FALSE(dimensions.Set(id, _T("bad")));
    EXPECT_EQ(2u, dimensions.Values().size());
}

TEST(ActionDimensions, ReplacesRemovesAndClearsValues)
{
    PiwikActionDimensions dimensions;
    dimensions.Set(1, _T("failure"));
    dimensions.Set(1, _T("success"));
    EXPECT_EQ(1u, dimensions.Values().size());
    EXPECT_EQ(_T("success"), dimensions.Values().at(1));
    EXPECT_TRUE(dimensions.Remove(1));
    EXPECT_FALSE(dimensions.Remove(1));
    dimensions.Set(2, _T("none"));
    dimensions.Clear();
    EXPECT_TRUE(dimensions.Values().empty());
}

TEST(ActionDimensions, OwnsCopiesAndHasNoLegacySlotLimitOrTruncation)
{
    auto state = Measurement();
    TSTRING value(500, _T('x'));
    for (int id = 1; id <= 20; ++id) ASSERT_TRUE(state.ActionDimensions.Set(id, value));
    value.clear();
    auto copy = state;
    state.ActionDimensions.Clear();
    EXPECT_EQ(20u, copy.ActionDimensions.Values().size());
    EXPECT_EQ(500u, copy.ActionDimensions.Values().at(20).size());
    EXPECT_NE(std::string::npos, copy.Serialize(PIWIK_FORMAT_URL).find("&dimension20=" + std::string(500, 'x')));
}

TEST_P(Dimensions, EncodesSpecialCharactersWithoutLiteralWrapperQuotes)
{
    auto state = Measurement();
    state.ActionDimensions.Set(2, _T("a &b=+%\"\\\n\r\t"));
    const auto query = state.Serialize(GetParam());
    EXPECT_NE(std::string::npos, query.find("&dimension2=a%20%26b%3D%2B%25%22%5C%0A%0D%09"));
    EXPECT_EQ(std::string::npos, query.find('"'));
    EXPECT_EQ(std::string::npos, query.find("&b="));
}

TEST_P(Dimensions, PreservesUnicodeAsUtf8)
{
#ifdef UNICODE
    auto state = Measurement();
    state.ActionDimensions.Set(9, L"\u05E9\u00E9\U0001F680");
    EXPECT_NE(std::string::npos, state.Serialize(GetParam()).find("&dimension9=%D7%A9%C3%A9%F0%9F%9A%80"));
#else
    GTEST_SKIP() << "Unicode API test";
#endif
}

TEST_P(Dimensions, ExplicitEmptyIsDifferentFromRemoved)
{
    auto state = Measurement();
    state.ActionDimensions.Set(5, _T(""));
    EXPECT_NE(std::string::npos, state.Serialize(GetParam()).find("&dimension5="));
    state.ActionDimensions.Remove(5);
    EXPECT_EQ(std::string::npos, state.Serialize(GetParam()).find("&dimension5="));
}

TEST_P(Dimensions, EmitsOneKeyWithTypedThenLegacyActionThenVisitPrecedence)
{
    auto state = Measurement();
    state.VistDimensionVariables.Items[0].Set(_T("dimension2"), _T("visit"));
    state.DimensionVariables.Items[0].Set(_T("dimension2"), _T("legacy"));
    state.ActionDimensions.Set(2, _T("typed"));
    auto query = state.Serialize(GetParam());
    EXPECT_EQ(1u, Count(query, "&dimension2="));
    EXPECT_NE(std::string::npos, query.find("&dimension2=typed"));
    state.ActionDimensions.Remove(2);
    query = state.Serialize(GetParam());
    EXPECT_EQ(1u, Count(query, "&dimension2="));
    EXPECT_NE(std::string::npos, query.find("&dimension2=legacy"));
    state.DimensionVariables.Items[0].Value.clear();
    EXPECT_NE(std::string::npos, state.Serialize(GetParam()).find("&dimension2=visit"));
}

TEST_P(Dimensions, FixesLegacyActionAndVisitEncoding)
{
    auto state = Measurement();
    state.DimensionVariables.Items[0].Set(_T("dimension2"), _T("Win32 & HRESULT"));
    state.VistDimensionVariables.Items[0].Set(_T("dimension3"), _T("A+B=\"C\""));
    const auto query = state.Serialize(GetParam());
    EXPECT_NE(std::string::npos, query.find("&dimension2=Win32%20%26%20HRESULT"));
    EXPECT_NE(std::string::npos, query.find("&dimension3=A%2BB%3D%22C%22"));
    EXPECT_EQ(std::string::npos, query.find('"'));
}

TEST_P(Dimensions, FailureContextDoesNotLeakIntoAnotherMeasurement)
{
    auto failure = Measurement();
    failure.ActionDimensions.Set(1, _T("failure"));
    failure.ActionDimensions.Set(2, _T("Win32"));
    failure.ActionDimensions.Set(3, _T("5"));
    auto success = Measurement();
    success.ActionDimensions.Set(1, _T("success"));
    success.ActionDimensions.Set(2, _T("none"));
    success.ActionDimensions.Set(3, _T("none"));
    const auto successQuery = success.Serialize(GetParam());
    EXPECT_NE(std::string::npos, successQuery.find("&dimension1=success"));
    EXPECT_NE(std::string::npos, successQuery.find("&dimension2=none"));
    EXPECT_NE(std::string::npos, successQuery.find("&dimension3=none"));
    EXPECT_NE(std::string::npos, failure.Serialize(GetParam()).find("&dimension3=5"));
}

INSTANTIATE_TEST_SUITE_P(QueryFormats, Dimensions,
    ::testing::Values(PIWIK_FORMAT_URL, PIWIK_FORMAT_JSON));
