#include <gmock/gmock.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <limits>
#include <thread>
#include <vector>
#include <future>
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

struct SendGate
{
    std::promise<void> entered, release;
    std::shared_future<void> released = release.get_future().share();
    std::atomic<bool> opened{false};
    bool WaitEntered() { return entered.get_future().wait_for(std::chrono::seconds(3)) == std::future_status::ready; }
    void Open() { if (!opened.exchange(true)) release.set_value(); }
    HttpResponse Send()
    {
        entered.set_value();
        EXPECT_EQ(std::future_status::ready, released.wait_for(std::chrono::seconds(5)));
        return Status(204);
    }
};

struct ReleaseSend
{
    std::shared_ptr<SendGate> gate;
    ~ReleaseSend() { gate->Open(); }
};

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

TEST_F(DispatcherTest, GetDimensionsAreEncodedAndCapturedAtSubmission)
{
    HttpRequest captured;
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([&](const HttpRequest& request) {
        captured = request;
        return Status(204);
    }));
    dispatcher->SetRequestMethod(PIWIK_METHOD_GET);
    auto state = Event();
    state.ActionDimensions.Set(3, _T("Win32 & HRESULT"));
    const int serial = dispatcher->Submit(state);
    state.ActionDimensions.Clear();
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(serial, 1);
    EXPECT_THAT(captured.path, HasSubstr(L"&dimension3=Win32%20%26%20HRESULT"));
}

TEST_F(DispatcherTest, BulkEventsKeepIndependentErrorContextSnapshots)
{
    HttpRequest captured;
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([&](const HttpRequest& request) {
        captured = request;
        return Status(204);
    }));
    auto state = Event();
    state.ActionDimensions.Set(1, _T("failure"));
    state.ActionDimensions.Set(2, _T("Win32"));
    state.ActionDimensions.Set(3, _T("5 & 6"));
    const int failure = dispatcher->Submit(state);
    state.ActionDimensions.Set(1, _T("success"));
    state.ActionDimensions.Set(2, _T("none"));
    state.ActionDimensions.Set(3, _T("none"));
    const int success = dispatcher->Submit(state);
    state.ActionDimensions.Clear();
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(success, 1);
    ExpectStatus(failure, 1);
    EXPECT_THAT(captured.body, HasSubstr("&dimension1=failure&dimension2=Win32&dimension3=5%20%26%206\",\"?idsite="));
    EXPECT_THAT(captured.body, HasSubstr("&dimension1=success&dimension2=none&dimension3=none\"]}"));
}

TEST_F(DispatcherTest, BulkBodyContainsEquivalentGetQueriesWithSpecialCharacters)
{
    HttpRequest captured;
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([&](const HttpRequest& request) {
        captured = request;
        return Status(204);
    }));
    auto first = Event(_T("a&b=+%\"\\\n"));
    first.TrackedPath = _T("https://example.invalid/?a=1&b=2");
    first.UserVariables.Items[2].Set(_T("name\""), _T("value&=+%\\\n"));
    first.ScreenVariables = first.UserVariables;
    auto second = Event(_T("literal%26+"));
    second.ActionDimensions.Set(2, _T("Win32 & HRESULT"));
    const auto firstGetQuery = first.Serialize(PIWIK_FORMAT_URL);
    const auto secondGetQuery = second.Serialize(PIWIK_FORMAT_URL);
    const int firstId = dispatcher->Submit(first);
    const int secondId = dispatcher->Submit(second);
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(secondId, 1);
    ExpectStatus(firstId, 1);
    // No inner quote/backslash/control bytes remain to break the outer JSON.
    for (const auto& query : {firstGetQuery, secondGetQuery})
    {
        EXPECT_EQ(std::string::npos, query.find_first_of("\"\\\r\n\t"));
        for (unsigned char byte : query) { EXPECT_GE(byte, 0x20); EXPECT_LT(byte, 0x7F); }
    }
    EXPECT_THAT(captured.body, HasSubstr("&e_n=a%26b%3D%2B%25%22%5C%0A"));
    EXPECT_EQ("{\"requests\":[\"" + firstGetQuery + "\",\"" + secondGetQuery + "\"]}", captured.body);
}

TEST_F(DispatcherTest, FlushAndWaitSucceedsWithoutCreatingAWorkerForAnEmptyQueue)
{
    EXPECT_CALL(*transport, Send(_)).Times(0);
    EXPECT_EQ(PiwikFlushResult::Success, dispatcher->FlushAndWait(std::chrono::milliseconds(0)));
    EXPECT_FALSE(dispatcher->Flush());
}

TEST_F(DispatcherTest, FlushAndWaitCompletesSuccessfulBulkAndCanBeRepeated)
{
    EXPECT_CALL(*transport, Send(_)).WillOnce(Return(Status(204)));
    auto state = Event();
    const int first = dispatcher->Submit(state);
    const int second = dispatcher->Submit(state);
    EXPECT_EQ(PiwikFlushResult::Success, dispatcher->FlushAndWait(std::chrono::seconds(3)));
    EXPECT_EQ(1, dispatcher->RequestStatus(first));
    EXPECT_EQ(1, dispatcher->RequestStatus(second));
    EXPECT_EQ(PiwikFlushResult::Success, dispatcher->FlushAndWait(std::chrono::milliseconds(0)));
}

TEST_F(DispatcherTest, FlushAndWaitHandlesVeryLargeTimeoutWithoutOverflow)
{
    EXPECT_CALL(*transport, Send(_)).WillOnce(Return(Status(204)));
    auto state = Event();
    dispatcher->Submit(state);
    EXPECT_EQ(PiwikFlushResult::Success, dispatcher->FlushAndWait((std::chrono::milliseconds::max)()));
}

TEST_F(DispatcherTest, FlushAndWaitIncludesEarlierFailuresAfterLaterSuccess)
{
    dispatcher->SetRequestMethod(PIWIK_METHOD_GET);
    EXPECT_CALL(*transport, Send(_)).WillOnce(Return(Status(500))).WillOnce(Return(Status(204)));
    auto state = Event();
    const int first = dispatcher->Submit(state);
    const int second = dispatcher->Submit(state);
    EXPECT_EQ(PiwikFlushResult::Failed, dispatcher->FlushAndWait(std::chrono::seconds(3)));
    EXPECT_EQ(-1, dispatcher->RequestStatus(first));
    EXPECT_EQ(1, dispatcher->RequestStatus(second));
    EXPECT_EQ(PiwikFlushResult::Failed, dispatcher->FlushAndWait(std::chrono::milliseconds(0)));
}

TEST_F(DispatcherTest, FlushAndWaitReportsTransportFailureAndDoesNotRetry)
{
    auto response = Status(0);
    response.errorCode = ERROR_WINHTTP_TIMEOUT;
    EXPECT_CALL(*transport, Send(_)).WillOnce(Return(response));
    auto state = Event();
    dispatcher->Submit(state);
    EXPECT_EQ(PiwikFlushResult::Failed, dispatcher->FlushAndWait(std::chrono::seconds(3)));
}

TEST_F(DispatcherTest, FlushAndWaitWaitsForAnInflightRequest)
{
    auto gate = std::make_shared<SendGate>();
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([gate](const HttpRequest&) { return gate->Send(); }));
    auto state = Event();
    const int serial = dispatcher->Submit(state);
    auto wait = std::async(std::launch::async, [&] { return dispatcher->FlushAndWait(std::chrono::seconds(3)); });
    ReleaseSend release{gate};
    ASSERT_TRUE(gate->WaitEntered());
    EXPECT_EQ(std::future_status::timeout, wait.wait_for(std::chrono::milliseconds(0)));
    EXPECT_EQ(0, dispatcher->RequestStatus(serial));
    gate->Open();
    EXPECT_EQ(PiwikFlushResult::Success, wait.get());
}

TEST_F(DispatcherTest, FlushAndWaitTimesOutWithoutCancellingTheSend)
{
    auto gate = std::make_shared<SendGate>();
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([gate](const HttpRequest&) { return gate->Send(); }));
    auto state = Event();
    const int serial = dispatcher->Submit(state);
    ASSERT_TRUE(dispatcher->Flush());
    ReleaseSend release{gate};
    ASSERT_TRUE(gate->WaitEntered());
    const auto start = std::chrono::steady_clock::now();
    EXPECT_EQ(PiwikFlushResult::TimedOut, dispatcher->FlushAndWait(std::chrono::milliseconds(20)));
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(2));
    EXPECT_EQ(PiwikFlushResult::TimedOut, dispatcher->FlushAndWait(std::chrono::milliseconds(-1)));
    EXPECT_EQ(0, dispatcher->RequestStatus(serial));
    gate->Open();
    EXPECT_EQ(PiwikFlushResult::Success, dispatcher->FlushAndWait(std::chrono::seconds(3)));
}

TEST_F(DispatcherTest, FlushAndWaitExcludesLaterRequestsEvenWhenTheirSendIsBlocked)
{
    dispatcher->SetRequestMethod(PIWIK_METHOD_GET);
    auto first = std::make_shared<SendGate>();
    auto later = std::make_shared<SendGate>();
    EXPECT_CALL(*transport, Send(_))
        .WillOnce(Invoke([first](const HttpRequest&) { return first->Send(); }))
        .WillOnce(Invoke([later](const HttpRequest&) { return later->Send(); }));
    auto state = Event();
    dispatcher->Submit(state);
    auto wait = std::async(std::launch::async, [&] { return dispatcher->FlushAndWait(std::chrono::seconds(3)); });
    ReleaseSend releaseFirst{first}, releaseLater{later};
    ASSERT_TRUE(first->WaitEntered()); // FlushAndWait captured its target before starting this send.
    const int laterId = dispatcher->Submit(state);
    first->Open();
    ASSERT_TRUE(later->WaitEntered());
    ASSERT_EQ(std::future_status::ready, wait.wait_for(std::chrono::seconds(2)));
    EXPECT_EQ(PiwikFlushResult::Success, wait.get());
    EXPECT_EQ(0, dispatcher->RequestStatus(laterId));
    later->Open();
    EXPECT_EQ(PiwikFlushResult::Success, dispatcher->FlushAndWait(std::chrono::seconds(3)));
}

TEST_F(DispatcherTest, FlushAndWaitSeparatesPostBatchesForDifferentEndpoints)
{
    ::testing::InSequence sequence;
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([](const HttpRequest& request) {
        EXPECT_EQ(L"example.invalid", request.host);
        EXPECT_THAT(request.body, HasSubstr("&e_n=first"));
        EXPECT_EQ(std::string::npos, request.body.find("&e_n=second"));
        return Status(204);
    }));
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([](const HttpRequest& request) {
        EXPECT_EQ(L"other.invalid", request.host);
        EXPECT_THAT(request.body, HasSubstr("&e_n=second"));
        return Status(204);
    }));
    auto first = Event(_T("first"));
    dispatcher->Submit(first);
    dispatcher->SetApiUrl(_T("https://other.invalid/piwik.php"));
    auto second = Event(_T("second"));
    dispatcher->Submit(second);
    EXPECT_EQ(PiwikFlushResult::Success, dispatcher->FlushAndWait(std::chrono::seconds(3)));
}

TEST_F(DispatcherTest, MultipleFlushWaitersReceiveCompletionNotifications)
{
    auto gate = std::make_shared<SendGate>();
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([gate](const HttpRequest&) { return gate->Send(); }));
    auto state = Event();
    dispatcher->Submit(state);
    auto first = std::async(std::launch::async, [&] { return dispatcher->FlushAndWait(std::chrono::seconds(3)); });
    auto second = std::async(std::launch::async, [&] { return dispatcher->FlushAndWait(std::chrono::seconds(3)); });
    ReleaseSend release{gate};
    ASSERT_TRUE(gate->WaitEntered());
    gate->Open();
    EXPECT_EQ(PiwikFlushResult::Success, first.get());
    EXPECT_EQ(PiwikFlushResult::Success, second.get());
}

TEST_F(DispatcherTest, FlushAndWaitDoesNotLosePostsAcrossMethodAndEndpointTransitions)
{
    ::testing::InSequence sequence;
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([](const HttpRequest& request) {
        EXPECT_EQ(L"POST", request.method);
        EXPECT_EQ(L"example.invalid", request.host);
        return Status(204);
    }));
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([](const HttpRequest& request) {
        EXPECT_EQ(L"GET", request.method);
        return Status(204);
    }));
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([](const HttpRequest& request) {
        EXPECT_EQ(L"POST", request.method);
        EXPECT_EQ(L"other.invalid", request.host);
        return Status(204);
    }));
    auto state = Event();
    dispatcher->Submit(state);
    dispatcher->SetRequestMethod(PIWIK_METHOD_GET);
    dispatcher->Submit(state);
    dispatcher->SetRequestMethod(PIWIK_METHOD_POST);
    ASSERT_TRUE(dispatcher->SetApiUrl(_T("https://other.invalid/piwik.php")));
    dispatcher->Submit(state);
    EXPECT_EQ(PiwikFlushResult::Success, dispatcher->FlushAndWait(std::chrono::seconds(3)));
}

TEST_F(DispatcherTest, ShutdownDrainIsTerminalAndReportsDeliveryFailure)
{
    EXPECT_CALL(*transport, Send(_)).WillOnce(Return(Status(500)));
    auto state = Event();
    const int serial = dispatcher->Submit(state);
    EXPECT_EQ(PiwikFlushResult::Failed, dispatcher->Shutdown());
    EXPECT_EQ(-1, dispatcher->RequestStatus(serial));
    EXPECT_EQ(0, dispatcher->Submit(state));
    EXPECT_FALSE(dispatcher->Flush());
    EXPECT_EQ(PiwikFlushResult::Failed, dispatcher->FlushAndWait(std::chrono::milliseconds(0)));
    EXPECT_EQ(PiwikFlushResult::Failed, dispatcher->Shutdown());
}

TEST_F(DispatcherTest, ShutdownDiscardMarksQueuedRequestsFailedWithoutSending)
{
    EXPECT_CALL(*transport, Send(_)).Times(0);
    auto state = Event();
    const int first = dispatcher->Submit(state);
    const int second = dispatcher->Submit(state);
    EXPECT_EQ(PiwikFlushResult::Failed, dispatcher->Shutdown(PiwikShutdownMode::DiscardPending));
    EXPECT_EQ(-1, dispatcher->RequestStatus(first));
    EXPECT_EQ(-1, dispatcher->RequestStatus(second));
    EXPECT_EQ(0, dispatcher->Submit(state));
    EXPECT_EQ(PiwikFlushResult::Failed, dispatcher->Shutdown(PiwikShutdownMode::Drain));
}

TEST_F(DispatcherTest, DestructorDrainsQueuedRequestsEvenImmediatelyAfterStartingWorker)
{
    bool delivered = false;
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([&](const HttpRequest&) {
        delivered = true;
        return Status(204);
    }));
    auto state = Event();
    dispatcher->Submit(state);
    dispatcher.reset(); // Join synchronizes the captured flag and transport destruction.
    EXPECT_TRUE(delivered);
}

TEST_F(DispatcherTest, DestructorJoinsAnIdleWorker)
{
    EXPECT_CALL(*transport, Send(_)).WillOnce(Return(Status(204)));
    auto state = Event();
    dispatcher->Submit(state);
    ASSERT_EQ(PiwikFlushResult::Success, dispatcher->FlushAndWait(std::chrono::seconds(3)));
    dispatcher.reset();
}

TEST_F(DispatcherTest, DestructorWaitsForInflightTransportBeforeDestroyingIt)
{
    auto gate = std::make_shared<SendGate>();
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([gate](const HttpRequest&) { return gate->Send(); }));
    auto state = Event();
    dispatcher->Submit(state);
    ASSERT_TRUE(dispatcher->Flush());
    ReleaseSend release{gate};
    ASSERT_TRUE(gate->WaitEntered());
    auto destroy = std::async(std::launch::async, [&] { dispatcher.reset(); });
    ReleaseSend releaseAgain{gate};
    EXPECT_EQ(std::future_status::timeout, destroy.wait_for(std::chrono::milliseconds(0)));
    gate->Open();
    destroy.get();
}

TEST_F(DispatcherTest, EmptyShutdownIsIdempotentAndDoesNotLaunchWorker)
{
    EXPECT_CALL(*transport, Send(_)).Times(0);
    EXPECT_EQ(PiwikFlushResult::Success, dispatcher->Shutdown());
    EXPECT_EQ(PiwikFlushResult::Success, dispatcher->Shutdown(PiwikShutdownMode::DiscardPending));
    auto state = Event();
    EXPECT_EQ(0, dispatcher->Submit(state));
    EXPECT_FALSE(dispatcher->Flush());
}

TEST_F(DispatcherTest, ShutdownDrainWaitsForInflightAndThenSendsQueuedWork)
{
    dispatcher->SetRequestMethod(PIWIK_METHOD_GET);
    auto gate = std::make_shared<SendGate>();
    EXPECT_CALL(*transport, Send(_))
        .WillOnce(Invoke([gate](const HttpRequest& request) {
            EXPECT_FALSE(request.cancellation->load());
            return gate->Send();
        })).WillOnce(Return(Status(204)));
    auto state = Event();
    dispatcher->Submit(state);
    ASSERT_TRUE(dispatcher->Flush());
    ReleaseSend release{gate};
    ASSERT_TRUE(gate->WaitEntered());
    const int queued = dispatcher->Submit(state);
    auto stop = std::async(std::launch::async, [&] { return dispatcher->Shutdown(); });
    // Guard declared after future: release the fake send on an early assertion.
    ReleaseSend releaseAgain{gate};
    EXPECT_EQ(std::future_status::timeout, stop.wait_for(std::chrono::milliseconds(0)));
    gate->Open();
    EXPECT_EQ(PiwikFlushResult::Success, stop.get());
    EXPECT_EQ(1, dispatcher->RequestStatus(queued));
}

TEST_F(DispatcherTest, ShutdownDiscardJoinsInflightBeforeReturningAndNotifiesFlushWaiters)
{
    dispatcher->SetRequestMethod(PIWIK_METHOD_GET);
    auto gate = std::make_shared<SendGate>();
    auto token = std::make_shared<std::shared_ptr<std::atomic<bool>>>();
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([gate, token](const HttpRequest& request) {
        *token = request.cancellation;
        auto response = gate->Send();
        if ((*token)->load()) response.errorCode = ERROR_OPERATION_ABORTED;
        return response;
    }));
    auto state = Event();
    const int active = dispatcher->Submit(state);
    ASSERT_TRUE(dispatcher->Flush());
    ReleaseSend release{gate};
    ASSERT_TRUE(gate->WaitEntered());
    const int queued = dispatcher->Submit(state);
    auto flush = std::async(std::launch::async, [&] { return dispatcher->FlushAndWait(std::chrono::seconds(3)); });
    auto stop = std::async(std::launch::async, [&] { return dispatcher->Shutdown(PiwikShutdownMode::DiscardPending); });
    ReleaseSend releaseAgain{gate};
    ExpectStatus(queued, -1); // Shutdown has discarded the queue and set cancellation.
    EXPECT_TRUE((*token)->load());
    EXPECT_EQ(std::future_status::timeout, stop.wait_for(std::chrono::milliseconds(0)));
    EXPECT_EQ(0, dispatcher->RequestStatus(active));
    gate->Open();
    EXPECT_EQ(PiwikFlushResult::Failed, stop.get());
    EXPECT_EQ(PiwikFlushResult::Failed, flush.get());
    EXPECT_EQ(-1, dispatcher->RequestStatus(active));
}

TEST_F(DispatcherTest, ConnectionTimeoutUsesFiniteValidatedSecondsForEachSend)
{
    dispatcher->SetConnectionTimeout(2);
    dispatcher->SetConnectionTimeout(0);
    dispatcher->SetConnectionTimeout(-1);
    dispatcher->SetConnectionTimeout((std::numeric_limits<int>::max)());
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([](const HttpRequest& request) {
        EXPECT_EQ(2000, request.timeoutMilliseconds);
        EXPECT_TRUE(request.cancellation);
        EXPECT_FALSE(request.cancellation->load());
        return Status(204);
    }));
    auto state = Event();
    dispatcher->Submit(state);
    EXPECT_EQ(PiwikFlushResult::Success, dispatcher->Shutdown());
}

TEST(WinHttpShutdown, AlreadyCancelledRequestDoesNotStartNetworkIo)
{
    piwik::detail::WinHttpTransport transport;
    HttpRequest request;
    request.cancellation = std::make_shared<std::atomic<bool>>(true);
    EXPECT_EQ(ERROR_OPERATION_ABORTED, transport.Send(request).errorCode);
}

TEST(WinHttpShutdown, InfiniteAndNegativePhaseTimeoutsAreRejectedBeforeNetworkIo)
{
    piwik::detail::WinHttpTransport transport;
    HttpRequest request;
    for (int timeout : {0, -1})
    {
        request.timeoutMilliseconds = timeout;
        EXPECT_EQ(ERROR_INVALID_PARAMETER, transport.Send(request).errorCode);
    }
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

TEST_F(DispatcherTest, InvalidNumericRequestsDoNotReachTransportOrConsumeIds)
{
    EXPECT_CALL(*transport, Send(_)).WillOnce(Return(Status(204)));
    for (double value : {std::numeric_limits<double>::quiet_NaN(),
                        std::numeric_limits<double>::infinity(),
                        -std::numeric_limits<double>::infinity()})
    {
        auto invalid = Event();
        invalid.SetEventValue(value);
        EXPECT_EQ(0, dispatcher->Submit(invalid));
    }
    EXPECT_FALSE(dispatcher->Flush());
    auto valid = Event();
    valid.SetEventValue(0.0);
    const int serial = dispatcher->Submit(valid);
    EXPECT_EQ(1, serial);
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(serial, 1);
}

TEST_F(DispatcherTest, ExplicitZeroReachesTheBulkRequest)
{
    EXPECT_CALL(*transport, Send(_)).WillOnce(Invoke([](const HttpRequest& request) {
        EXPECT_THAT(request.body, HasSubstr("&e_v=0\"]}"));
        return Status(204);
    }));
    auto state = Event();
    state.SetEventValue(0.0);
    const int serial = dispatcher->Submit(state);
    ASSERT_TRUE(dispatcher->Flush());
    ExpectStatus(serial, 1);
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
