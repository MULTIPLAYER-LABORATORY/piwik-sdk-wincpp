#include <gtest/gtest.h>
#include <limits>
#include <sstream>
#include "Piwik.h"

// Construct the real client to verify the complete static library links, without
// tracking anything, starting a dispatcher worker, or contacting a server.
TEST(Client, ConfigurationWorksWithoutSendingRequests)
{
    PiwikClient client(_T("https://example.invalid/piwik.php"), 7);
    EXPECT_EQ(7, client.CurrentSiteId());
    EXPECT_TRUE(client.UsesSecureConnection());
    EXPECT_TRUE(client.VerifiesServerCertificate());
    client.SetVerifyServerCertificate(false);
    EXPECT_FALSE(client.VerifiesServerCertificate());
    client.SetVerifyServerCertificate(true);
    EXPECT_TRUE(client.VerifiesServerCertificate());
    client.SetDisabled(true);
    EXPECT_TRUE(client.IsDisabled());
    client.SetSiteId(12);
    EXPECT_EQ(12, client.CurrentSiteId());
}

TEST(Client, CertificatePolicyIsIndependentForEachClient)
{
    PiwikClient first(_T("https://example.invalid/piwik.php"), 7);
    PiwikClient second(_T("https://example.invalid/piwik.php"), 8);
    first.SetVerifyServerCertificate(false);
    EXPECT_FALSE(first.VerifiesServerCertificate());
    EXPECT_TRUE(second.VerifiesServerCertificate());
}

TEST(Client, NonFiniteNumbersAreRejectedBeforeCreatingRequests)
{
    PiwikClient client(_T("https://example.invalid/piwik.php"), 7);
    client.SetDryRun(true);
    for (float value : {std::numeric_limits<float>::quiet_NaN(),
                       std::numeric_limits<float>::infinity(),
                       -std::numeric_limits<float>::infinity()})
    {
        EXPECT_EQ(0, client.TrackEvent(_T("/startup"), _T("Performance"),
            _T("Startup"), _T("ui_ready_ms"), value));
        EXPECT_EQ(0, client.TrackGoal(_T("/goal"), 1, value));
    }
    EXPECT_FALSE(client.Flush()); // No worker or request was created.
}

TEST(Client, LowLevelExplicitZeroSurvivesClientEnrichment)
{
    std::wostringstream log; // Outlives the client and its logger.
    PiwikClient client(_T("https://example.invalid/piwik.php"), 7);
    client.SetDispatchInterval(3600);
    client.SetDryRun(true);
    client.SetLogger(&log, PIWIK_LOG_DEBUG);
    PiwikState state;
    state.TrackedPath = _T("/startup");
    state.EventCategory = _T("Performance");
    state.EventAction = _T("Startup");
    state.EventName = _T("ui_ready_ms");
    state.SetEventValue(0.0);
    EXPECT_GT(client.Track(state), 0);
    EXPECT_NE(std::wstring::npos, log.str().find(L"&e_v=0"));
}

TEST(Client, PerEventDimensionsSurviveEnrichmentWithoutBecomingVisitState)
{
    std::wostringstream log;
    PiwikClient client(_T("https://example.invalid/piwik.php"), 7);
    client.SetDispatchInterval(3600);
    client.SetDryRun(true);
    client.SetLogger(&log, PIWIK_LOG_DEBUG);
    client.SetVisitDimensions(1, _T("dimension9"), _T("Windows"));
    PiwikState state;
    state.TrackedPath = _T("/startup");
    state.EventCategory = _T("Performance");
    state.EventAction = _T("Startup");
    state.ActionDimensions.Set(2, _T("Win32 & HRESULT"));
    EXPECT_GT(client.Track(state), 0);
    EXPECT_NE(std::wstring::npos, log.str().find(L"&dimension2=Win32%20%26%20HRESULT&dimension9=Windows"));
    log.str(L"");
    EXPECT_GT(client.TrackEvent(_T("/event"), _T("Demo"), _T("Open")), 0);
    EXPECT_EQ(std::wstring::npos, log.str().find(L"&dimension2="));
    EXPECT_NE(std::wstring::npos, log.str().find(L"&dimension9=Windows"));
}

TEST(Client, LegacyTrackActionUsesCorrectDimensionWireValues)
{
    std::wostringstream log;
    PiwikClient client(_T("https://example.invalid/piwik.php"), 7);
    client.SetDispatchInterval(3600);
    client.SetDryRun(true);
    client.SetLogger(&log, PIWIK_LOG_DEBUG);
    EXPECT_GT(client.TrackAction(_T("/install"), _T("Install"), 0, 1,
        _T("dimension2"), _T("Win32 & HRESULT")), 0);
    EXPECT_NE(std::wstring::npos, log.str().find(L"&dimension2=Win32%20%26%20HRESULT"));
}

TEST(Client, LegacyEventCallsStillOmitMissingAndZeroValues)
{
    std::wostringstream log;
    PiwikClient client(_T("https://example.invalid/piwik.php"), 7);
    client.SetDispatchInterval(3600);
    client.SetDryRun(true);
    client.SetLogger(&log, PIWIK_LOG_DEBUG);
    EXPECT_GT(client.TrackEvent(_T("/event"), _T("Demo"), _T("Open"), _T("omitted")), 0);
    EXPECT_GT(client.TrackEvent(_T("/event"), _T("Demo"), _T("Open"), _T("zero"), 0.0f), 0);
    EXPECT_EQ(std::wstring::npos, log.str().find(L"&e_v="));
}
