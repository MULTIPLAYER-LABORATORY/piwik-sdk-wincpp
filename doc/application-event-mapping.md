# Mapping application activity to Matomo events

This is an optional application schema, not a reserved SDK vocabulary. Keep your
application's events meaningful to your product, and translate them into Matomo
fields in a small application-owned adapter. The SDK handles serialization and
delivery; it does not decide which application activities constitute success or
failure. No particular application, site ID, or dimension ID is required.

## Suggested vocabulary

Choose stable operation names such as `Startup`, `Import`, and `Export`. Keep
capitalization and units consistent across releases. This example uses schema
version `1`:

| Application activity | Category | Action | Name | Numeric value |
| --- | --- | --- | --- | --- |
| Application started | `Lifecycle` | `Started` | Application name | Omitted |
| Normal application shutdown | `Lifecycle` | `Stopped` | Exit reason, e.g. `UserExit` | Omitted |
| Operation completed | `Operation` | `Completed` | Operation name | Omitted |
| Operation failed | `Error` | Stable error code, e.g. `NETWORK_TIMEOUT` | Operation name | Omitted |
| Duration measured | `Performance` | Operation name | Metric name, e.g. `duration_ms` | Milliseconds |

Define when each event fires. For example, `Started` means application entry,
while `Startup/ui_ready_ms` ends only when the main interface is usable. An SDK
initialization measurement is a different metric, `tracking_init_ms`. Record a
normal stop before shutting down the tracker; an absent stop event alone does
not prove a crash. A process crash may prevent queued telemetry from being sent.

For a successful operation, optionally emit `Operation/Completed` and one
performance event. For failure, emit one error event and, if useful, one
performance event with the failure outcome. These are different event records:
do not add their counts together to calculate the number of operations. Record
one terminal outcome per operation attempt. Treat cancellation as `cancelled`,
not automatically as an error; an application can add `Operation/Cancelled`.

## Context dimensions

Configure these dimensions on the Matomo site before sending them. The IDs below
are examples; map them to the actual site's IDs in your application adapter.
Using Action scope for this example keeps every event's context self-contained.

| Example ID | Dimension | Scope | Example values |
| --- | --- | --- | --- |
| 1 | Outcome | Action | `success`, `failure`, `cancelled`, `none` |
| 2 | Error domain | Action | `Application`, `Win32`, `HTTP`, `none` |
| 3 | Error code | Action | `NETWORK_TIMEOUT`, `5`, `503`, `none` |
| 4 | Severity | Action | `info`, `warning`, `error` |
| 5 | Application version | Action | `2.4.0` |
| 6 | Schema version | Action | `1` |

Keep error codes as strings paired with a domain. For example, `Win32/5` and
`HTTP/503` have different meanings. Prefer a small, documented set of application
codes over changing exception text. A team may put the stable application code
in the event action and the underlying platform code in dimension 3.

Use `none` on non-error events so an intentional absence is explicit. Build a
fresh state/context for each event; reusing a failed operation's context can
incorrectly attach its error to a later success. For properties genuinely shared
by a visit, Visit dimensions are another option, with separate configured IDs.
Matomo determines scope from its configuration, not from the C++ variable name.

## C++14 example

This example uses the installed static library and the default Unicode package.
It deliberately runs in dry-run mode and contacts no server. Replace the endpoint,
site ID, dimension IDs, version, and simulated work before enabling delivery.
The helper is application code, not an additional SDK API.

```cpp
#include <Piwik.h>
#include <chrono>

PiwikActionDimensions MakeContext(
    LPCTSTR outcome, LPCTSTR domain, LPCTSTR code, LPCTSTR severity)
{
    PiwikActionDimensions context;
    context.Set(1, outcome);
    context.Set(2, domain);
    context.Set(3, code);
    context.Set(4, severity);
    context.Set(5, _T("2.4.0"));
    context.Set(6, _T("1"));
    return context;
}

int RecordEvent(PiwikClient& tracker, LPCTSTR path, LPCTSTR category,
    LPCTSTR action, LPCTSTR name, const PiwikActionDimensions& context)
{
    PiwikState event;
    event.TrackedPath = path;
    event.EventCategory = category;
    event.EventAction = action;
    event.EventName = name;
    event.CustomAction = true;
    event.ActionDimensions = context;
    // No SetEventValue call: these events have no numeric measurement.
    return tracker.Track(event);
}

int main()
{
    PiwikClient tracker(_T("https://analytics.example/matomo.php"), 1);
    tracker.SetDryRun(true); // Remove only after configuring your Matomo site.

    const auto info = MakeContext(
        _T("none"), _T("none"), _T("none"), _T("info"));
    RecordEvent(tracker, _T("/lifecycle"), _T("Lifecycle"),
        _T("Started"), _T("ExampleApp"), info);

    const auto start = std::chrono::steady_clock::now();
    // Perform real initialization here, ending when the UI is usable.
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const auto success = MakeContext(
        _T("success"), _T("none"), _T("none"), _T("info"));
    const int timingId = tracker.TrackDuration(
        _T("/startup"), _T("Startup"), _T("ui_ready_ms"), elapsed, success);

    RecordEvent(tracker, _T("/operations/import"), _T("Operation"),
        _T("Completed"), _T("Import"), success);

    // A separate, illustrative failed Export attempt.
    const auto failure = MakeContext(
        _T("failure"), _T("HTTP"), _T("503"), _T("error"));
    RecordEvent(tracker, _T("/operations/export"), _T("Error"),
        _T("SERVICE_UNAVAILABLE"), _T("Export"), failure);

    RecordEvent(tracker, _T("/lifecycle"), _T("Lifecycle"),
        _T("Stopped"), _T("UserExit"), info);
    const auto stopped = tracker.Shutdown(PiwikShutdownMode::Drain);
    const auto delivery = tracker.RequestResult(timingId);
    return stopped == PiwikFlushResult::Success &&
        delivery.state == PiwikDeliveryState::Succeeded && delivery.dryRun
        ? 0 : 1;
}
```

`TrackDuration` converts chrono durations to fractional milliseconds, includes
explicit zero measurements, and rejects negative/nonfinite durations. It measures
nothing automatically. Capture the endpoint before submitting telemetry. For
other numeric metrics, use `PiwikState::SetEventValue(double)`, including explicit
zero; the legacy `TrackEvent` convenience method omits zero values.

The final `delivery.dryRun` check is intentional for this demonstration. Remove
that requirement when adapting the example for real delivery. Return values from
tracking calls are local request IDs; zero means no request was queued. Neither
an ID nor an HTTP success proves that Matomo stored the intended fields.
`Shutdown(Drain)` joins the worker and has no hard total deadline. See the
[delivery and shutdown documentation](../README.md#cooperative-shutdown) when
choosing the application's exit policy.

## Matomo fields and reports

The mapping uses `e_c` for category, `e_a` for action, `e_n` for name, and optional
`e_v` for a numeric value. Context uses `dimensionN`; these examples also set
`ca=1` for non-pageview actions. Pass original strings to the SDK without URL
encoding them first. See the [Matomo Tracking API](https://developer.matomo.org/api-reference/tracking-api).

For the startup example, the meaningful fields after query decoding are:

```text
e_c=Performance
e_a=Startup
e_n=ui_ready_ms
e_v=<elapsed milliseconds>
ca=1
dimension1=success
dimension2=none
dimension3=none
dimension4=info
dimension5=2.4.0
dimension6=1
```

Filter event reports by category, then operation and metric. Use event counts for
frequency and numeric values for measurements. A total of durations is not a
startup latency; compare averages within the same metric/unit and outcome, or
export measurements for distribution analysis. Never combine milliseconds with
byte counts or compare differently defined startup endpoints as one series.
Custom dimensions provide version/outcome/error context where the configured
reports support it. The mapping is lossy: Matomo events do not recreate an
application log or operation trace, and this schema provides no per-attempt
correlation identifier.

Validate a small known run in Matomo's visit/event data, including values and
dimensions, before relying on aggregate reports. Archive processing and report
configuration can affect when and where data appears. SDK `RequestResult`
describes telemetry transport failures; an application `Error` event describes
the monitored operation. Keep these two error channels separate, and avoid
recursively reporting a delivery failure through the same failing tracker.

## Keeping the schema useful

- Document each code's meaning, severity, and retry policy in the application.
  For example, `SERVICE_UNAVAILABLE` means the remote service rejected an Export
  attempt temporarily; retries are the application's decision.
- Keep labels bounded. Avoid timestamps, random IDs, full paths, usernames,
  credentials, and raw exception messages in category/action/name or dimensions.
  Put detailed diagnostics in the application's appropriate local logging system.
- If sampling or rate limiting is introduced, document it. Sampled error counts
  cannot be interpreted as every failure. This schema does not implement sampling.
- Preserve established label meanings. For an incompatible change, increment the
  schema version and document how to compare old and new data. Adding a new
  metric with a distinct name is usually simpler than redefining an old metric.
