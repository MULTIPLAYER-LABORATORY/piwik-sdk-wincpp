#include <gtest/gtest.h>
#include <chrono>
#include <limits>
#include <sstream>
#include "Piwik.h"

namespace {
using Milliseconds = std::chrono::duration<double, std::milli>;
class Duration : public ::testing::Test
{
protected:
    Duration() : client(_T("https://example.invalid/piwik.php"), 7)
    {
        client.SetDryRun(true);
        client.SetDispatchInterval(3600);
        client.SetLogger(&log, PIWIK_LOG_DEBUG);
    }
    std::wostringstream log; // Outlives the client.
    PiwikClient client;
};
}

TEST_F(Duration, MapsToAnExplicitPerformanceEventWithoutLegacyTiming)
{
    EXPECT_GT(client.TrackDuration(_T("/startup"), _T("Startup"), _T("ui_ready_ms"), Milliseconds(1250.25)), 0);
    const auto query = log.str();
    EXPECT_NE(std::wstring::npos, query.find(L"&e_c=Performance&e_a=Startup&e_n=ui_ready_ms&e_v=1250.25&ca=1"));
    EXPECT_EQ(std::wstring::npos, query.find(L"&gt_ms="));
    EXPECT_EQ(std::wstring::npos, query.find(L"&pf_"));
}

TEST_F(Duration, ConvertsStandardChronoUnitsAndKeepsZeroAndFractions)
{
    struct Sample { Milliseconds elapsed; const wchar_t* expected; };
    for (const auto& sample : {Sample{std::chrono::seconds(2), L"&e_v=2000&ca=1"},
                              Sample{std::chrono::microseconds(1250250), L"&e_v=1250.25&ca=1"},
                              Sample{std::chrono::nanoseconds(125000), L"&e_v=0.125&ca=1"},
                              Sample{std::chrono::milliseconds(0), L"&e_v=0&ca=1"}})
    {
        log.str(L"");
        EXPECT_GT(client.TrackDuration(_T("/operation"), _T("Open"), _T("duration_ms"), sample.elapsed), 0);
        EXPECT_NE(std::wstring::npos, log.str().find(sample.expected));
    }
}

TEST_F(Duration, AcceptsSteadyClockDifferencesWithoutMeasuringNetworkTime)
{
    const auto start = std::chrono::steady_clock::time_point{};
    const auto end = start + std::chrono::microseconds(1500);
    EXPECT_GT(client.TrackDuration(_T("/startup"), _T("Startup"), _T("ready_ms"), end - start), 0);
    EXPECT_NE(std::wstring::npos, log.str().find(L"&e_v=1.5&ca=1"));
}

TEST_F(Duration, RejectsNegativeNonfiniteAndOverflowedDurationsBeforeQueuing)
{
    for (double value : {-1.0, std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        EXPECT_EQ(0, client.TrackDuration(_T("/startup"), _T("Startup"), _T("ready_ms"), Milliseconds(value)));
    EXPECT_EQ(0, client.TrackDuration(_T("/startup"), _T("Startup"), _T("ready_ms"),
        std::chrono::duration<double, std::ratio<3600>>((std::numeric_limits<double>::max)())));
    EXPECT_FALSE(client.Flush());
    EXPECT_EQ(std::wstring::npos, log.str().find(L"Submitting query"));
}

TEST_F(Duration, RejectsNullEmptyAndBlankLabelsBeforeQueuing)
{
    for (LPCTSTR bad : {static_cast<LPCTSTR>(nullptr), _T(""), _T(" \t\r\n\v\f")})
    {
        EXPECT_EQ(0, client.TrackDuration(bad, _T("Startup"), _T("ready_ms"), Milliseconds(1)));
        EXPECT_EQ(0, client.TrackDuration(_T("/startup"), bad, _T("ready_ms"), Milliseconds(1)));
        EXPECT_EQ(0, client.TrackDuration(_T("/startup"), _T("Startup"), bad, Milliseconds(1)));
    }
    EXPECT_FALSE(client.Flush());
}

TEST_F(Duration, CopiesErrorContextWithoutChangingTheCallerOrVisitDefaults)
{
    client.SetVisitDimensions(1, _T("dimension9"), _T("Windows"));
    PiwikActionDimensions context;
    context.Set(1, _T("failure"));
    context.Set(2, _T("Win32 & HRESULT"));
    context.Set(3, _T("0x80070005"));
    EXPECT_GT(client.TrackDuration(_T("/install"), _T("Install"), _T("duration_ms"), Milliseconds(2), context), 0);
    EXPECT_EQ(3u, context.Values().size());
    EXPECT_NE(std::wstring::npos, log.str().find(L"&dimension1=failure&dimension2=Win32%20%26%20HRESULT&dimension3=0x80070005&dimension9=Windows"));
    context.Clear();
    log.str(L"");
    EXPECT_GT(client.TrackDuration(_T("/startup"), _T("Startup"), _T("ready_ms"), Milliseconds(0)), 0);
    EXPECT_EQ(std::wstring::npos, log.str().find(L"&dimension1="));
    EXPECT_NE(std::wstring::npos, log.str().find(L"&dimension9=Windows"));
}

TEST_F(Duration, UsesTheExistingDisabledAndSiteConfigurationChecks)
{
    client.SetDisabled(true);
    EXPECT_EQ(0, client.TrackDuration(_T("/startup"), _T("Startup"), _T("ready_ms"), Milliseconds(1)));
    client.SetDisabled(false);
    client.SetSiteId(0);
    EXPECT_EQ(0, client.TrackDuration(_T("/startup"), _T("Startup"), _T("ready_ms"), Milliseconds(1)));
    EXPECT_FALSE(client.Flush());
}

TEST_F(Duration, EncodesLabelsEquallyInGetAndBulk)
{
    for (auto method : {PIWIK_METHOD_GET, PIWIK_METHOD_POST})
    {
        client.SetRequestMethod(method);
        log.str(L"");
        EXPECT_GT(client.TrackDuration(_T("/a?b=1&c=2"), _T("Open & Load"), _T("a+b_ms"), Milliseconds(0.5)), 0);
        EXPECT_NE(std::wstring::npos, log.str().find(L"&e_c=Performance&e_a=Open%20%26%20Load&e_n=a%2Bb_ms&e_v=0.5&ca=1"));
    }
}
