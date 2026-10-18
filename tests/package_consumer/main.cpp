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
    return client.CurrentSiteId() == 7 && client.UsesSecureConnection() &&
        client.VerifiesServerCertificate() ? 0 : 3;
}
