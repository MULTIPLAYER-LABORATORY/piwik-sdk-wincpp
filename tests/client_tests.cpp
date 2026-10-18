#include <gtest/gtest.h>
#include "Piwik.h"

// Construct the real client to verify the complete static library links, without
// tracking anything, starting a dispatcher worker, or contacting a server.
TEST(Client, ConfigurationWorksWithoutSendingRequests)
{
    PiwikClient client(_T("https://example.invalid/piwik.php"), 7);
    EXPECT_EQ(7, client.CurrentSiteId());
    EXPECT_TRUE(client.UsesSecureConnection());
    client.SetDisabled(true);
    EXPECT_TRUE(client.IsDisabled());
    client.SetSiteId(12);
    EXPECT_EQ(12, client.CurrentSiteId());
}
