#include <gtest/gtest.h>
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
