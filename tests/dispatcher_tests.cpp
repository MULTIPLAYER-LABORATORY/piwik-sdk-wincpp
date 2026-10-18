#include <gmock/gmock.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include "Piwik.h"
#include "HttpTransport.h"

namespace {
using piwik::detail::HttpRequest;
using piwik::detail::HttpResponse;
using piwik::detail::IHttpTransport;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Return;

class MockHttpTransport : public IHttpTransport
{
public:
    MOCK_METHOD(HttpResponse, Send, (const HttpRequest& request), (override));
};

HttpResponse Status(unsigned int code)
{
    HttpResponse response;
    response.statusCode = code;
    return response;
}

class DispatcherTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto mock = std::make_unique<::testing::StrictMock<MockHttpTransport>>();
        transport = mock.get();
        dispatcher = std::make_unique<PiwikDispatcher>(std::move(mock));
        ASSERT_TRUE(dispatcher->SetApiUrl(_T("https://example.invalid/piwik.php")));
        // Queue deterministically; only an explicit Flush wakes the worker.
        dispatcher->SetDispatchInterval(3600);
    }

    PiwikState Event(LPCTSTR name = _T("App"))
    {
        PiwikState state;
        state.SiteId = 7;
        state.TrackedPath = _T("/screen");
        state.UserAgent.clear();
        state.Random = 42;
        state.EventCategory = _T("Demo");
        state.EventAction = _T("Launch");
        state.EventName = name;
        state.EventValue = 2.5f;
        return state;
    }

    void ExpectStatus(int serial, int expected)
    {
        // Wait on observable completion, not an assumed worker scheduling delay.
        // Mutex-protected RequestStatus synchronizes access to mock captures.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        int status;
        do {
            status = dispatcher->RequestStatus(serial);
            if (status != 0) break;
            std::this_thread::yield();
        } while (std::chrono::steady_clock::now() < deadline);
        ASSERT_EQ(expected, status);
    }

    MockHttpTransport* transport = nullptr;
    std::unique_ptr<PiwikDispatcher> dispatcher;
};

TEST_F(DispatcherTest, GetRequestContainsEncodedQueryAndNoBody)
{
    HttpRequest captured;
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([&](const HttpRequest& request) {
        captured = request;
        return Status(204);
    }));
    dispatcher->SetRequestMethod(PIWIK_METHOD_GET);
    auto state = Event();
    const int serial = dispatcher->Submit(state);
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(serial, 1);
    EXPECT_EQ(L"GET", captured.method);
    EXPECT_EQ(L"example.invalid", captured.host);
    EXPECT_TRUE(captured.secure);
    EXPECT_TRUE(captured.verifyServerCertificate);
    EXPECT_TRUE(captured.body.empty());
    EXPECT_THAT(captured.path, HasSubstr(L"/piwik.php?idsite=7&url=%2Fscreen"));
    EXPECT_THAT(captured.path, HasSubstr(L"&e_c=Demo&e_a=Launch&e_n=App&e_v=2.5"));
}

TEST_F(DispatcherTest, PostRequestCarriesBulkJsonAndRetainsEndpoint)
{
    HttpRequest captured;
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([&](const HttpRequest& request) {
        captured = request;
        return Status(200);
    }));
    auto state = Event();
    const int serial = dispatcher->Submit(state);
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(serial, 1);
    EXPECT_EQ(L"POST", captured.method);
    EXPECT_EQ(L"/piwik.php", captured.path);
    EXPECT_EQ(L"Content-Type:application/json;charset=UTF-8\r\n", captured.headers);
    EXPECT_THAT(captured.body, HasSubstr("{\"requests\":[\"?idsite=7&url="));
    EXPECT_THAT(captured.body, HasSubstr("&e_n=App&e_v=2.5\"]}"));
}

struct StatusCase { unsigned int code; int expected; };
class DeliveryStatusTest : public DispatcherTest,
    public ::testing::WithParamInterface<StatusCase> {};

TEST_P(DeliveryStatusTest, ClassifiesHttpResponse)
{
    EXPECT_CALL(*transport, Send(_)).WillOnce(Return(Status(GetParam().code)));
    auto state = Event();
    const int serial = dispatcher->Submit(state);
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(serial, GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(HttpStatuses, DeliveryStatusTest, ::testing::Values(
    StatusCase{200, 1}, StatusCase{204, 1}, StatusCase{299, 1},
    StatusCase{302, -1}, StatusCase{400, -1}, StatusCase{500, -1}, StatusCase{503, -1}),
    [](const ::testing::TestParamInfo<StatusCase>& info) {
        return "Http" + std::to_string(info.param.code);
    });

TEST_F(DispatcherTest, NetworkFailureIsReportedWithoutRetry)
{
    auto response = Status(0);
    response.errorCode = ERROR_WINHTTP_TIMEOUT;
    response.errorMessage = L"Could not send HTTP request";
    EXPECT_CALL(*transport, Send(_)).WillOnce(Return(response));
    auto state = Event();
    const int serial = dispatcher->Submit(state);
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(serial, -1);
}

TEST_F(DispatcherTest, TransportErrorTakesPrecedenceOverSuccessStatus)
{
    auto response = Status(200);
    response.errorCode = ERROR_WINHTTP_CONNECTION_ERROR;
    EXPECT_CALL(*transport, Send(_)).WillOnce(Return(response));
    auto state = Event();
    const int serial = dispatcher->Submit(state);
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(serial, -1);
}

TEST_F(DispatcherTest, DryRunAcknowledgesWithoutCallingTransport)
{
    EXPECT_CALL(*transport, Send(_)).Times(0);
    dispatcher->SetDryRun(true);
    auto state = Event();
    const int serial = dispatcher->Submit(state);
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(serial, 1);
}

TEST_F(DispatcherTest, BatchAcknowledgesEveryQueuedEvent)
{
    HttpRequest captured;
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([&](const HttpRequest& request) {
        captured = request;
        return Status(200);
    }));
    auto first = Event(_T("first")), second = Event(_T("second"));
    const int one = dispatcher->Submit(first);
    const int two = dispatcher->Submit(second);
    EXPECT_EQ(0, dispatcher->RequestStatus(two));
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(two, 1);
    ExpectStatus(one, 1);
    EXPECT_EQ(2, std::count(captured.body.begin(), captured.body.end(), '?'));
    EXPECT_THAT(captured.body, HasSubstr("&e_n=first"));
    EXPECT_THAT(captured.body, HasSubstr("&e_n=second"));
}

TEST_F(DispatcherTest, FailedBatchMarksEveryEventFailed)
{
    EXPECT_CALL(*transport, Send(_)).WillOnce(Return(Status(500)));
    auto state = Event();
    const int one = dispatcher->Submit(state);
    const int two = dispatcher->Submit(state);
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(two, -1);
    ExpectStatus(one, -1);
}

TEST_F(DispatcherTest, PostBatchesRespectConfiguredBundleLimit)
{
    std::vector<HttpRequest> captured;
    EXPECT_CALL(*transport, Send(_)).Times(2).WillRepeatedly(Invoke([&](const HttpRequest& request) {
        captured.push_back(request);
        return Status(200);
    }));
    auto state = Event();
    int last = 0;
    for (int i = 0; i < PIWIK_POST_BUNDLE + 1; ++i)
        last = dispatcher->Submit(state);
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(last, 1);
    ASSERT_EQ(2u, captured.size());
    EXPECT_EQ(PIWIK_POST_BUNDLE, std::count(captured[0].body.begin(), captured[0].body.end(), '?'));
    EXPECT_EQ(1, std::count(captured[1].body.begin(), captured[1].body.end(), '?'));
}

TEST_F(DispatcherTest, LaterSuccessDoesNotHideEarlierFailure)
{
    dispatcher->SetRequestMethod(PIWIK_METHOD_GET);
    ::testing::InSequence sequence;
    EXPECT_CALL(*transport, Send(_)).WillOnce(Return(Status(500)));
    EXPECT_CALL(*transport, Send(_)).WillOnce(Return(Status(204)));
    auto state = Event();
    const int one = dispatcher->Submit(state);
    const int two = dispatcher->Submit(state);
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(two, 1);
    ExpectStatus(one, -1);
}

TEST_F(DispatcherTest, HttpEndpointDoesNotSetSecureFlag)
{
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([](const HttpRequest& request) {
        EXPECT_FALSE(request.secure);
        return Status(204);
    }));
    ASSERT_TRUE(dispatcher->SetApiUrl(_T("http://example.invalid/piwik.php")));
    auto state = Event();
    const int serial = dispatcher->Submit(state);
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(serial, 1);
}

TEST_F(DispatcherTest, ExplicitCertificateOptOutReachesTransport)
{
    dispatcher->SetVerifyServerCertificate(false);
    EXPECT_FALSE(dispatcher->VerifiesServerCertificate());
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([](const HttpRequest& request) {
        EXPECT_TRUE(request.secure);
        EXPECT_FALSE(request.verifyServerCertificate);
        return Status(204);
    }));
    auto state = Event();
    const int serial = dispatcher->Submit(state);
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(serial, 1);
}

TEST_F(DispatcherTest, CertificateValidationCanBeRestoredForSubsequentSends)
{
    ::testing::InSequence sequence;
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([](const HttpRequest& request) {
        EXPECT_FALSE(request.verifyServerCertificate);
        return Status(204);
    }));
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([](const HttpRequest& request) {
        EXPECT_TRUE(request.verifyServerCertificate);
        return Status(204);
    }));
    dispatcher->SetVerifyServerCertificate(false);
    auto state = Event();
    const int first = dispatcher->Submit(state);
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(first, 1);
    dispatcher->SetVerifyServerCertificate(true);
    const int second = dispatcher->Submit(state);
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(second, 1);
}

TEST_F(DispatcherTest, CertificateFailureDoesNotDisableValidationOrRetry)
{
    auto failure = Status(0);
    failure.errorCode = ERROR_WINHTTP_SECURE_FAILURE;
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([&](const HttpRequest& request) {
        EXPECT_TRUE(request.verifyServerCertificate);
        return failure;
    }));
    auto state = Event();
    const int serial = dispatcher->Submit(state);
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(serial, -1);
    EXPECT_TRUE(dispatcher->VerifiesServerCertificate());
}

TEST_F(DispatcherTest, DisablingValidationWritesWarningToConfiguredLogger)
{
    std::wostringstream log;
    dispatcher->SetLogger(&log, PIWIK_LOG_ERROR);
    dispatcher->SetVerifyServerCertificate(false);
    EXPECT_THAT(log.str(), HasSubstr(L"WARNING: Server certificate validation disabled"));
    dispatcher->SetLogger(nullptr, PIWIK_LOG_ERROR);
}

TEST(WinHttpCertificatePolicy, VerificationIsEnabledByDefault)
{
    HttpRequest request;
    request.secure = true;
    EXPECT_TRUE(request.verifyServerCertificate);
    EXPECT_EQ(0u, piwik::detail::CertificateSecurityFlags(request));
}

TEST(WinHttpCertificatePolicy, ExplicitHttpsOptOutUsesLegacyIgnoreFlags)
{
    HttpRequest request;
    request.secure = true;
    request.verifyServerCertificate = false;
    const DWORD expected = SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
        SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_UNKNOWN_CA |
        SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
    EXPECT_EQ(expected, piwik::detail::CertificateSecurityFlags(request));
}

TEST(WinHttpCertificatePolicy, HttpDoesNotApplyCertificateIgnoreFlags)
{
    HttpRequest request;
    request.secure = false;
    request.verifyServerCertificate = false;
    EXPECT_EQ(0u, piwik::detail::CertificateSecurityFlags(request));
}
} // namespace
