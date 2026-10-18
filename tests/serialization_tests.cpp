#include <gtest/gtest.h>
#include "Piwik.h"

namespace {
PiwikState BasicState()
{
    PiwikState state;
    state.SiteId = 7;
    state.TrackedPath = _T("https://example.invalid/startup");
    state.UserAgent.clear();
    state.Random = 42;
    return state;
}
}

TEST(Serialization, RequiredParametersHaveExpectedWireFormat)
{
    auto state = BasicState();
    EXPECT_EQ("?idsite=7&url=https%3A%2F%2Fexample.invalid%2Fstartup&rec=1&send_image=0&rand=42&apiv=1",
              state.Serialize(PIWIK_FORMAT_URL));
}

TEST(Serialization, EventContainsCategoryActionNameAndFractionalValue)
{
    auto state = BasicState();
    state.EventCategory = _T("Performance");
    state.EventAction = _T("Startup");
    state.EventName = _T("ui_ready_ms");
    state.EventValue = 1250.5f;
    const auto query = state.Serialize(PIWIK_FORMAT_URL);
    EXPECT_NE(std::string::npos, query.find("&e_c=Performance&e_a=Startup&e_n=ui_ready_ms&e_v=1250.5"));
}

TEST(Serialization, AbsentOptionalFieldsAreOmitted)
{
    auto state = BasicState();
    const auto query = state.Serialize(PIWIK_FORMAT_URL);
    EXPECT_EQ(std::string::npos, query.find("&e_c="));
    EXPECT_EQ(std::string::npos, query.find("&uid="));
    EXPECT_EQ(std::string::npos, query.find("&dimension"));
}

TEST(Serialization, UserAndVisitorIdentifiersRemainSeparateFields)
{
    auto state = BasicState();
    state.UserId = _T("user 1");
    state.VisitorId = _T("0123456789abcdef");
    const auto query = state.Serialize(PIWIK_FORMAT_URL);
    EXPECT_NE(std::string::npos, query.find("&uid=user%201&_id=0123456789abcdef"));
}

TEST(Serialization, VisitDimensionsUseConfiguredIdsAndEncodeValues)
{
    auto state = BasicState();
    state.VistDimensionVariables.Items[0].Set(_T("dimension2"), _T("Windows & app"));
    const auto query = state.Serialize(PIWIK_FORMAT_URL);
    EXPECT_NE(std::string::npos, query.find("&dimension2=Windows%20%26%20app"));
}

TEST(Serialization, BulkRequestStringEscapesQuotesAndBackslashes)
{
    auto state = BasicState();
    state.EventName = _T("a\"b\\c");
    const auto query = state.Serialize(PIWIK_FORMAT_JSON);
    EXPECT_NE(std::string::npos, query.find("&e_n=a\\\"b\\\\c"));
}
