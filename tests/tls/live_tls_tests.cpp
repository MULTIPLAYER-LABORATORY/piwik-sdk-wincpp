#include <gtest/gtest.h>
#include <cstdlib>
#include <cwchar>
#include "HttpTransport.h"

namespace {
using piwik::detail::HttpRequest;
using piwik::detail::WinHttpTransport;

class LiveTlsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        wchar_t portText[16] = {};
        const auto length = GetEnvironmentVariableW(L"PIWIK_TEST_TLS_PORT", portText, 16);
        ASSERT_GT(length, 0u) << "Run through the loopback fixture script";
        ASSERT_LT(length, 16u);
        const auto port = std::wcstoul(portText, nullptr, 10);
        ASSERT_GT(port, 0u);
        ASSERT_LE(port, 65535u);
        request.host = L"127.0.0.1";
        request.port = static_cast<INTERNET_PORT>(port);
        request.path = L"/";
        request.method = L"GET";
        request.secure = true;
    }
    HttpRequest request;
    WinHttpTransport transport;
};

TEST_F(LiveTlsTest, DefaultRejectsUntrustedCertificate)
{
    const auto response = transport.Send(request);
    EXPECT_EQ(ERROR_WINHTTP_SECURE_FAILURE, response.errorCode);
    EXPECT_EQ(0u, response.statusCode);
}

TEST_F(LiveTlsTest, ExplicitOptOutAllowsSelfSignedServer)
{
    request.verifyServerCertificate = false;
    const auto response = transport.Send(request);
    EXPECT_EQ(ERROR_SUCCESS, response.errorCode) << response.errorMessage;
    EXPECT_EQ(204u, response.statusCode);
}

TEST_F(LiveTlsTest, RestoringValidationRejectsPreviouslyAcceptedServer)
{
    request.verifyServerCertificate = false;
    const auto relaxed = transport.Send(request);
    ASSERT_EQ(ERROR_SUCCESS, relaxed.errorCode) << relaxed.errorMessage;
    ASSERT_EQ(204u, relaxed.statusCode);
    request.verifyServerCertificate = true;
    const auto strict = transport.Send(request);
    EXPECT_EQ(ERROR_WINHTTP_SECURE_FAILURE, strict.errorCode);
    EXPECT_EQ(0u, strict.statusCode);
}
TEST_F(LiveTlsTest, StalledResponseHonorsFiniteHttpPhaseTimeout)
{
    request.verifyServerCertificate = false;
    request.path = L"/stall";
    request.timeoutMilliseconds = 1000;
    const auto response = transport.Send(request);
    EXPECT_EQ(ERROR_WINHTTP_TIMEOUT, response.errorCode) << response.errorMessage;
    EXPECT_EQ(0u, response.statusCode);
}
} // namespace
