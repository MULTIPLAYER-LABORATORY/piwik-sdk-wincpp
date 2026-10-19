#include <Piwik.h>

int main()
{
    PiwikClient client(_T("https://example.invalid/piwik.php"), 7);
    if (!client.VerifiesServerCertificate()) return 1;
    client.SetVerifyServerCertificate(false);
    if (client.VerifiesServerCertificate()) return 2;
    client.SetVerifyServerCertificate(true);
    PiwikState measurement;
    measurement.SetEventValue(0.0);
    if (!measurement.HasEventValue || !measurement.HasFiniteNumbers()) return 4;
    measurement.ClearEventValue();
    if (measurement.HasEventValue) return 5;
    if (!measurement.ActionDimensions.Set(2, _T("Win32"))) return 6;
    if (measurement.ActionDimensions.Set(0, _T("invalid"))) return 7;
    if (measurement.Serialize(PIWIK_FORMAT_JSON).find("&dimension2=Win32") == std::string::npos) return 8;
    if (!measurement.ActionDimensions.Remove(2)) return 9;
    measurement.ActionDimensions.Clear();
    measurement.EventName = _T("a&b=+%\"\\\n");
    measurement.ScreenVariables.Items[0].Set(_T("name\""), _T("value&+"));
    const auto query = measurement.Serialize(PIWIK_FORMAT_JSON);
    if (query != measurement.Serialize(PIWIK_FORMAT_URL)) return 10;
    if (query.find("&e_n=a%26b%3D%2B%25%22%5C%0A") == std::string::npos) return 11;
    if (query.find("&cvar=%7B%22") == std::string::npos) return 12;
    // Exercise the installed chrono API without contacting any server.
    client.SetDryRun(true);
    client.SetDispatchInterval(3600);
    if (client.RequestResult(0).state != PiwikDeliveryState::Unknown) return 17;
    PiwikActionDimensions context;
    context.Set(1, _T("success"));
    if (client.TrackDuration(_T("/startup"), _T("Startup"), _T("ready_ms"),
        std::chrono::microseconds(1250), context) <= 0) return 13;
    if (client.FlushAndWait(std::chrono::seconds(3)) != PiwikFlushResult::Success) return 14;
    const auto delivered = client.RequestResult(1);
    if (delivered.state != PiwikDeliveryState::Succeeded || !delivered.dryRun ||
        delivered.httpStatus != 0 || delivered.errorCode != ERROR_SUCCESS) return 18;
    if (client.Shutdown(PiwikShutdownMode::Drain) != PiwikFlushResult::Success) return 15;
    if (client.Shutdown(PiwikShutdownMode::DiscardPending) != PiwikFlushResult::Success) return 16;
    return client.CurrentSiteId() == 7 && client.UsesSecureConnection() &&
        client.VerifiesServerCertificate() ? 0 : 3;
}
