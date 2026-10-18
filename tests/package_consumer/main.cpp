#include <Piwik.h>

int main()
{
    PiwikClient client(_T("https://example.invalid/piwik.php"), 7);
    return client.CurrentSiteId() == 7 && client.UsesSecureConnection() ? 0 : 1;
}
