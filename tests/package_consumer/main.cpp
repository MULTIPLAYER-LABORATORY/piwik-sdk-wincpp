#include <Piwik.h>

int main()
{
    PiwikClient client(_T("https://example.invalid/piwik.php"), 7);
    if (!client.VerifiesServerCertificate()) return 1;
    client.SetVerifyServerCertificate(false);
    if (client.VerifiesServerCertificate()) return 2;
    client.SetVerifyServerCertificate(true);
    return client.CurrentSiteId() == 7 && client.UsesSecureConnection() &&
        client.VerifiesServerCertificate() ? 0 : 3;
}
