# piwik-sdk-wincpp

## Piwik C++ tracking SDK for Windows Desktop

### Design

This SDK makes possible for a Windows Desktop application to access a Piwik web server in order to track GUI events and generate statistical data about its usage. All kind of events can be tracked as standardly supported by the Piwik API, although some of them must be interpreted somehow differently in the context of a desktop application.

The SDK is an application-independent native Windows C++ library for Matomo (formerly Piwik). The supported build baseline is Visual Studio 2019 or newer, C++14, CMake 3.21 or newer, and Ninja. Windows XP is no longer supported. The default Windows API target is Windows 7 (`0x0601`); this is an API baseline, not certification on older operating systems. Runtime validation currently targets the Windows versions used by CI and local development.

The production library depends only on the C++ runtime and Windows system libraries. GoogleTest/gMock is downloaded only when tests are enabled.

Reducing the footprint of the library to the absolute minimum has been another key objective, so that it can be used intensively without causing a noticeable overhead in resource consumption. Thanks to lightweight objects and direct execution paths tracking can be done under good network conditions without compromising the overall performance of the application.

### Basic usage

Include `Piwik.h` and link the static library `PiwikClient.lib` (Release) or `PiwikClientd.lib` (Debug). The library and consuming application must agree on architecture, character mode, configuration, compiler ABI, and C runtime. Presets default to Unicode and the static CRT (`/MT` in Release, `/MTd` in Debug); ANSI and dynamic CRT builds can be configured explicitly.

### Build and test

Run these commands in an x86 MSVC developer environment with CMake and Ninja on PATH:

```powershell
cmake --preset windows-x86-debug
cmake --build --preset windows-x86-debug
ctest --preset windows-x86-debug
cmake --install out/build/windows-x86-debug
```

Alternatively, from an ordinary PowerShell terminal, the helper locates Visual Studio 2019 and initializes the matching compiler environment:

```powershell
.\tools\build.ps1                         # x86 Debug: configure, build, test
.\tools\build.ps1 -Preset windows-x86-release
.\tools\build.ps1 -Action Install          # install previously built x86 Debug
```

For Visual Studio 2022, pass `-VisualStudioVersion '[17.0,18.0)'`. The helper uses CMake/Ninja from PATH or a Visual Studio installation. CMake must be at least 3.21. Presets do not select the compiler version themselves: it is supplied by the developer environment or helper. The external architecture hint is for IDEs; command-line builds validate the actual compiler architecture and fail if it does not match the preset.

Available configure/build/test presets are `windows-x86-debug`, `windows-x86-release`, `windows-x64-debug`, and `windows-x64-release`. Build trees are isolated under `out/build`, and packages install under `out/install`. Do not share a build tree across compiler versions or runtime choices.

To build just the SDK, with no test dependencies downloaded:

```powershell
cmake --preset windows-x86-release -DBUILD_TESTING=OFF
cmake --build --preset windows-x86-release
```

For a dynamic CRT variant, use a separate build directory and pass `-DCMAKE_MSVC_RUNTIME_LIBRARY='MultiThreaded$<$<CONFIG:Debug>:Debug>DLL'`. For ANSI, pass `-DPIWIK_UNICODE=OFF`. Match these choices in the application. Preset settings are reapplied when configuring, so maintain custom variants in ignored `CMakeUserPresets.json` or provide the overrides each time.

### Consuming the installed library

For a CMake application, set `CMAKE_PREFIX_PATH` to the installed package directory, then use:

```cmake
find_package(MatomoSDK CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE Matomo::SDK)
```

The imported target carries include paths, character-mode/API definitions, C++14 requirements, and Windows link dependencies. Configure the application's runtime library to match the SDK; this setting cannot be enforced by an imported static target.

For a Visual Studio application using its own project files, add the installed `include/matomo` directory to include paths and the installed `lib` directory to library paths. Link the appropriate SDK library plus `winhttp.lib`, `advapi32.lib`, and `user32.lib`. Define `UNICODE` and `_UNICODE` for the default package. SDK headers reference standard-library types, so rebuild and link with compatible MSVC settings.

### Unit tests and incremental changes

GoogleTest/gMock 1.14.0 is pinned by commit and built in C++14 mode with the same runtime as the SDK. Tests cover encoding, Unicode conversion, event serialization, visit dimensions, optional fields, and linking/configuring the real client. The helper and CI also install a package into an isolated directory, build a standalone consumer with `find_package`, and run it. Tests do not contact Matomo or write visitor registry state. Public tracking methods remain nonvirtual.

CTest also runs four real WinHTTP TLS checks through a temporary loopback server
on an automatically assigned port: rejection of an untrusted certificate,
successful delivery with explicit opt-out, and rejection after restoring
validation on the same transport, and timeout of a deliberately stalled response.
The fixture uses PowerShell (`pwsh`, or Windows
PowerShell with the required .NET certificate APIs), generates its own untrusted
certificate, and installs nothing in Windows certificate stores. Schannel uses a
temporary Windows-managed private key that is removed on certificate disposal.
No external server, firewall change, trusted certificate, or fixed port is needed.
The `tls_integration` CTest entry runs the three certificate tests and one timeout test; these are
separate from the unit-test entries.

For an offline test build, supply an existing checkout of the pinned GoogleTest commit through `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<path>`; the helper accepts extra configure options through `-ConfigureArguments`.

Network tests use the internal `piwik::detail::IHttpTransport` interface and an owning injection constructor on `PiwikDispatcher`. Production uses `WinHttpTransport`; gMock supplies controlled responses while tests exercise real serialization, batching, and request-status handling. Transport headers are not installed as part of the application API. Public tracking methods remain nonvirtual. Tests cover GET/POST request construction, HTTP success/error classification, transport errors, dry runs, batch limits, and per-request outcomes. They use bounded completion checks rather than fixed scheduling sleeps.

The Windows session is opened lazily on the first real send; session/connection/request handles are owned by the transport. HTTP-status-based success classification and no-retry behavior are retained. Shutdown now cooperatively joins the worker, as described below. Certificate validation defaults to enabled, with an explicit per-client opt-out. Queue/status accesses and terminal shutdown state are synchronized. GET/bulk query encoding and bulk JSON construction have regression coverage. Retry logic, interpretation of partial failures in server bulk responses, and broader configuration thread-safety changes remain separate increments.

### Inspect delivery results

`RequestResult(id)` returns a thread-safe, nonblocking `PiwikRequestResult` copy:

| Field | Meaning |
| --- | --- |
| `state` | `PiwikDeliveryState::Unknown`, `Pending`, `Succeeded`, `Failed`, or `Discarded` |
| `httpStatus` | HTTP response status, or zero when unavailable |
| `errorCode` | WinHTTP/Windows transport error, or `ERROR_SUCCESS` when none was reported |
| `message` | Wide-string diagnostic; use state/codes for program logic |
| `dryRun` | True when the request completed in dry-run mode without being sent |

```cpp
const auto result = tracker.RequestResult(id);
if (result.state == PiwikDeliveryState::Failed) {
    // Inspect result.httpStatus and result.errorCode; display result.message.
}
```

Unknown IDs include zero (tracking returned no queued request), negative IDs,
and IDs not yet issued by this client. Queued and in-flight requests are pending.
HTTP 2xx counts as success unless the transport reports an error. For example,
HTTP 500 is `Failed` with `httpStatus == 500` and usually `errorCode == 0`;
a timeout is `Failed` with `errorCode == ERROR_WINHTTP_TIMEOUT` (12002).
`DiscardPending` marks queued requests `Discarded`; an in-flight send cancelled
by the transport is `Failed` with `ERROR_OPERATION_ABORTED` (995).

Results remain queryable after shutdown and are retained until client destruction,
so storage grows with completed requests. Every ID in a POST batch receives the
same HTTP result; this does not prove individual Matomo ingestion. No retries or
response-body parsing are added. Diagnostics contain SDK/transport messages, not
server response bodies. Dry runs retain legacy success behavior with `dryRun == true`.
`RequestStatus` retains its existing integer behavior (including legacy handling
of invalid IDs); use the new method to identify unknown IDs. Rebuild headers and
the static library together. These delivery errors are separate from application
error codes sent as tracking events/dimensions.

Timing dependencies can be injected separately when session/retry behavior is tested. Numeric event handling now distinguishes explicit zero from a missing value while retaining the legacy convenience method's zero-as-absent behavior. Custom dimension values use URL encoding in both GET and bulk requests.

GitHub Actions builds and tests x86/x64 Debug/Release using the VS2019 v142 toolset. CMake presets replace the old checked-in Visual Studio 2010 project files as the maintained build entry point. The historical Windows GUI sample source remains for reference.

After instantiating the PiwikClient object, which is the main component providing tracking facilities to the application, a set of properties can be assigned to it in order to define the appropriate endpoints and customize its behavior. Different operation modes are selectable like using synchronous or asynchronous dispatching with regulable time frames, sending HTTP requests with GET or POST methods or using encrypted secure connections. A stream pointer can also be passed to the tracker to activate logging facilities. 

Direct entry points are provided to the hosting application to track standard targets in a convenient way: user events, screen views, goals, display or content interactions can be tracked by simply supplying the most relevant parameters for each one of them. For more advanced usages, a low-level call is also provided in which the whole set of trackable values is made accessible to the calling function thus allowing to track complex or non-standard situations. At any time pending events can be flushed to the server if required and tracking can be finished by simply deleting the PiwikClient instance.

### Interface

To get a detailed insight into the interface of the library take a look at the [Reference Documentation](doc/Reference.md).

### Server certificate validation

HTTPS server certificate validation is enabled by default. This changes the old
behavior, which silently ignored hostname, validity-date, issuer, and usage
errors. Applications should normally use a certificate trusted by Windows.

For a deployment that deliberately requires the old behavior, opt out on that
client instance before tracking:

```cpp
PiwikClient tracker(_T("https://analytics.example/piwik.php"), 1);
tracker.SetVerifyServerCertificate(false); // Explicit opt-out for this deployment
// tracker.VerifiesServerCertificate() now returns false.
```

Call `SetVerifyServerCertificate(true)` to restore validation. The option is
independent for each client. It affects subsequent sends, including requests
already queued; an in-flight request keeps the policy captured when it began.
The transport recreates its WinHTTP session when the verification policy changes
so it does not reuse connections from the previous policy.

If a logger is configured when validation is disabled, it receives a warning.
Certificate failures remain failed deliveries; the SDK never automatically
disables verification or retries with relaxed settings. Plain HTTP does not use
certificate validation, and this option does not make HTTPS work without a TLS
server certificate. The opt-out relaxes the four legacy certificate checks; it
does not disable TLS protocol or other security checks.

For a private/self-signed deployment, trusting the issuing CA in Windows allows
validation to remain enabled. Rebuild consumers with the updated headers and
static library together when adopting this version.

### Waiting for queued delivery

`Flush()` remains a nonblocking dispatch signal, not delivery confirmation. Use
`FlushAndWait(std::chrono::milliseconds timeout)` when an application needs to
wait for already-submitted telemetry, for example before destroying the client:

```cpp
const auto result = tracker.FlushAndWait(std::chrono::seconds(2));
if (result == PiwikFlushResult::TimedOut) {
    // Sends may still be running; decide how the application should proceed.
}
```

The call captures the highest submitted request ID and wakes the worker. It
waits for all requests through that ID, including in-flight requests. New requests
do not extend its target or get added across that boundary to its POST batch.
Concurrent callers can wait for their own captured targets. Waiting uses completion
notifications and a steady-clock deadline, with no network polling loop.

- `Success`: every request through the target completed with the SDK's existing
  HTTP-level success classification (or dry-run acknowledgement).
- `Failed`: those requests finished and at least one failed, or the worker could
  not be started/signalled. Failures are cumulative since client creation and are
  not cleared by flushing; a later success cannot hide an earlier failure.
- `TimedOut`: the target did not finish before the wait deadline. This takes
  precedence over known delivery failures when other target requests remain pending.

An empty client returns `Success` without creating a worker. Zero/negative
timeouts request dispatch and check completion without waiting. Disabling new
tracking does not prevent this method from draining requests already queued.
`RequestStatus(id)` still provides individual results. HTTP success does not prove
Matomo stored every bulk entry; server ingestion/partial-response validation is
separate work.

Timeout does not cancel a request, retry it, or remove it from the queue. A later
call can observe completion. This bounds the completion wait, not the entire
client destructor; cooperative shutdown below owns the final join. Keep the
client alive while calls are active and do not destroy it concurrently with
tracking or waiting. Rebuild headers and the static library together after dispatcher changes.

POST batches retain the existing size limit and now stop at GET/endpoint
transitions, ensuring that every submitted request is actually sent to its
captured host/path before being marked complete.

### Cooperative shutdown

`Shutdown(PiwikShutdownMode mode = PiwikShutdownMode::Drain)` permanently closes
the client to new requests, wakes its worker, waits for it to finish, and closes
thread/wake handles only after the join. The destructor uses `Drain` automatically.
There is no fixed startup sleep or forced `TerminateThread` fallback.

- `Drain` sends already-queued work and lets in-flight sends finish.
- `DiscardPending` marks unsent queued requests failed and requests cooperative
  cancellation of the in-flight send between WinHTTP phases. It still joins that
  worker; it never abandons a thread using client resources.

```cpp
// Optional earlier bounded wait; a timeout alone does not cancel anything.
const auto flushed = tracker.FlushAndWait(std::chrono::seconds(2));
const auto stopped = tracker.Shutdown(
    flushed == PiwikFlushResult::TimedOut
        ? PiwikShutdownMode::DiscardPending : PiwikShutdownMode::Drain);
```

Shutdown returns cumulative `Success` or `Failed`, using the existing HTTP/dry-run
classification and counting discarded requests as failures. It does not return
`TimedOut`: joining is required for safe resource ownership. `RequestStatus` and
`FlushAndWait` remain usable for results. Further tracking returns zero, even if
`SetDisabled(false)` is called; create a new client to resume tracking. Repeated
shutdown calls are safe, and concurrent shutdown calls are serialized. The first
call's policy governs its join; a later call does not interrupt a drain in progress.

`SetConnectionTimeout(int seconds)` now reaches WinHTTP and sets **each** resolve,
connect, send, and receive phase timeout (default five seconds). Values must be
positive and fit in signed milliseconds; invalid values are ignored and logged
when logging is configured. Settings apply to subsequent sends; a send already
in progress retains its timeout snapshot. The response-header completion timeout
is also configured explicitly. WinHTTP timers have coarse enforcement and can
report a timeout later than the configured duration; they are not precise clocks.

These phase limits are not an overall shutdown deadline: draining many batches
can take longer, and an active synchronous WinHTTP operation must return before
cancellation can be observed. Cancellation cannot retract data already accepted
by the server. Handles are never closed concurrently with blocked operations, following Microsoft's
[WinHTTP concurrency rules](https://learn.microsoft.com/en-us/windows/win32/winhttp/concurrency-in-winhttp)
and [timeout API](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpsettimeouts).
No retry is introduced.

Keep the object alive until all calls have returned. Do not destroy it concurrently
with other calls, or invoke shutdown/destruction from a transport/logger callback
on its own worker. Internal custom transports must eventually return and honor
the timeout/cancellation contract. An OS-level join error throws `std::system_error`
without releasing worker-owned resources; ordinary HTTP failures return `Failed`.
Rebuild headers and the static library together. `PIWIK_SHUTDOWN_WAIT` remains a
legacy constant and no longer imposes a forced-kill deadline. Live Matomo ingestion
remains untested.

### Tracking application durations

`TrackDuration` accepts a C++14 chrono duration and queues a Performance event
with the elapsed value in fractional milliseconds. Measure with a monotonic
clock and capture the end before calling the tracker:

```cpp
const auto start = std::chrono::steady_clock::now();
// Perform application initialization here.
const auto elapsed = std::chrono::steady_clock::now() - start;

PiwikActionDimensions context;
context.Set(1, _T("success")); // Replace with your configured Action dimension ID.
const int requestId = tracker.TrackDuration(
    _T("/startup"), _T("Startup"), _T("ui_ready_ms"), elapsed, context);
```

The mapping is `e_c=Performance`, `e_a=<operation>`, `e_n=<metric>`,
`e_v=<milliseconds>`, `ca=1`. Give metrics stable names with `_ms` units;
the method does not rename or append units to labels. Seconds, milliseconds,
microseconds, and clock differences convert to `duration<double, std::milli>`.
Zero is an explicit measurement; negative, NaN, infinite, or conversion-overflowed
values return zero without queuing. Null, empty, or ASCII-whitespace-only path,
operation, and metric labels also return zero.

The optional dimensions are copied per event; pass your configured outcome,
error source, and string error code there. No application-specific IDs or codes
are reserved. The return value is the existing request identifier (zero means
not queued), not proof of delivery; use `RequestStatus` for the existing
HTTP-level outcome. Default dispatch is asynchronous; existing dispatch and
synchronous-mode settings still apply. The method does not start a timer, wait
for completion, alter `gt_ms`, or emit `pf_*` page metrics. Failed or cancelled
operations can carry elapsed time plus their outcome dimensions.

`PiwikState::CustomAction` is available to low-level callers and defaults to false.
`TrackDuration` sets it true to emit `ca=1`; existing tracking methods retain
their wire behavior. Matomo documents this flag for non-pageview actions in the
[Tracking API](https://developer.matomo.org/api-reference/tracking-api).
Rebuild headers and the static library together after the tracking-state layout
change. Live Matomo ingestion remains untested until a test server is available.

### Request encoding

Pass strings in their original form. The SDK URL-encodes query values for both
GET requests and each query string in a bulk POST, as required by the
[Matomo Tracking API](https://developer.matomo.org/api-reference/tracking-api).
For example, an event name `a&b=+%` is sent as `e_n=a%26b%3D%2B%25` and arrives
as the original value after one query decode. Quotes, backslashes, control
characters, and literal percent escapes are preserved without introducing extra
parameters. Unicode builds convert text to UTF-8 before encoding; ANSI builds
retain their byte-string behavior.

Custom variable collections (`cvar` and `_cvar`) are first serialized as JSON,
with their names and values JSON-escaped, then URL-encoded as one query value.
Sparse variable slots retain their original one-based indices.

`PiwikState::Serialize(PIWIK_FORMAT_URL)` and `Serialize(PIWIK_FORMAT_JSON)` now
return the same encoded query string. The latter is a bulk-array entry, not an
entire JSON document; the dispatcher supplies the outer `requests` object.
Encoded queries contain only ASCII characters safe inside JSON strings.

This corrects old bulk encoding and GET custom-variable JSON. Requests containing
special characters may now produce corrected report values where old requests
were corrupted or rejected. Tracking signatures and parameter names are unchanged.
Tests check decoded GET/bulk equivalence and captured bulk bodies without requiring
a live Matomo instance; server-side ingestion and reports still need deployment
validation.

### Numeric event values

Tracking numbers use a decimal point and no thousands separators regardless of
the application's locale. Floating-point values retain round-trip precision on
the wire, including fractional measurements and scientific notation.

The existing `TrackEvent(..., float val = 0)` signature is unchanged: omitted
values and zero continue to omit `e_v`. For an explicit zero or full double
precision, use the low-level state API:

```cpp
PiwikState measurement;
measurement.TrackedPath = _T("/startup");
measurement.EventCategory = _T("Performance");
measurement.EventAction = _T("Startup");
measurement.EventName = _T("ui_ready_ms");
measurement.SetEventValue(0.0); // A measured zero, serialized as e_v=0
const int requestId = tracker.Track(measurement);
```

`SetEventValue(double)` records an explicit value; `ClearEventValue()` resets it
to absent. Existing nonzero assignments to `EventValue` still serialize, and a
direct zero assignment to a fresh state retains its legacy absent behavior.
`EventValue` is now a double; consumers must rebuild their headers and static
library together. The old convenience method still accepts float, so use the
state API when double precision matters.

NaN and positive/negative infinity are invalid for event values and revenue.
`Track` and dispatcher `Submit` return zero without queuing a request for invalid
numbers. `PiwikState::Serialize` throws `std::invalid_argument` for such states;
`HasFiniteNumbers()` permits a preflight check. Negative finite event values remain
supported. These rules do not change the application's global locale.

### Per-event dimensions for outcomes and error codes

Use `PiwikState::ActionDimensions` to attach application-defined context to one
event. Configure each ID with **Action** scope on your Matomo site first; IDs
and error vocabularies belong to the application. The SDK cannot verify the
server's dimension configuration. See the [Matomo Tracking API](https://developer.matomo.org/api-reference/tracking-api).

```cpp
PiwikState measurement;
measurement.TrackedPath = _T("/startup");
measurement.EventCategory = _T("Performance");
measurement.EventAction = _T("Startup");
measurement.EventName = _T("ui_ready_ms");
measurement.SetEventValue(1250.5);
// Example IDs only: replace with your site's configured Action dimensions.
measurement.ActionDimensions.Set(1, _T("failure"));
measurement.ActionDimensions.Set(2, _T("Win32"));
measurement.ActionDimensions.Set(3, _T("5"));
const int requestId = tracker.Track(measurement);
```

`PiwikActionDimensions::Set(int id, const TSTRING& value)` returns false for IDs
outside 1..999, leaving the collection unchanged. Setting the same ID replaces
its value. Values are owned strings, without the legacy fixed-slot limit or
client-side length truncation; the server's configured limits still apply.
`Values()` provides read-only access. `Remove(id)` omits that ID from subsequent
requests and returns whether it existed; `Clear()` omits all IDs in this collection.
An empty string is an explicit `dimensionN=` value, not removal. Neither removal
nor an empty value promises to clear data already stored on the server.

Each state owns its dimensions. `Track` serializes a snapshot before returning;
later changes to that state do not change queued requests or visit settings.
Use a new state for each operation, or replace/clear error fields before reuse.
For success, an application's schema can explicitly send `none` for error source
and code. Keep codes as strings, for example `0x80070005` or `5`, so their namespace
and representation are preserved; do not put codes into the numeric duration.

Dimensions from all sources are emitted once per identical wire key. Typed
`ActionDimensions` take precedence over the legacy per-action `DimensionVariables`
array, which takes precedence over visit defaults. Duplicate legacy keys use the
last value in their array. Scope is determined by Matomo configuration; assigning
the same ID to both visit and action context is not recommended.

This update also corrects the legacy dimension serializers: values no longer get
extra wrapper quotes, and both GET and bulk query strings URL-encode dimension
names/values, including `&`, `+`, quotes, and UTF-8 text. Reports may therefore
show corrected values separately from old values that contained literal quotes.
Existing variadic method signatures remain available. Rebuild headers and the
static library together after the state layout change.

### Legacy generation-time field (`gt_ms`)

Matomo 4 stopped recording the old page-generation-time metric; historical data
remains available in old reports. See the [Matomo platform changelog](https://developer.matomo.org/changelog).
The SDK retains `AmountOfTime`, `PARAM_AMOUNT_OF_TIME`, and the `amountOfTime`
arguments on legacy tracking methods for compatibility with older servers.
The arguments on `TrackScreen`/`TrackAction` are seconds converted to integer
milliseconds in `gt_ms`; the low-level `AmountOfTime` field is already milliseconds.

This timing path is legacy-only. New application durations should use named
numeric event values with a unit in the metric name, such as `ui_ready_ms` or
`install_duration_ms`. There is no automatic conversion of legacy page/action
requests into events, since that would change report meanings. Modern `pf_*`
fields describe web-page performance and have different semantics from native
application operation durations. Existing legacy wire output is retained and
covered by a regression test; removal is deferred to a compatibility-breaking release.

