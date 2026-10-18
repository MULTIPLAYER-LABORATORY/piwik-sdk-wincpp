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

CTest also runs three real WinHTTP TLS checks through a temporary loopback server
on an automatically assigned port: rejection of an untrusted certificate,
successful delivery with explicit opt-out, and rejection after restoring
validation on the same transport. The fixture uses PowerShell (`pwsh`, or Windows
PowerShell with the required .NET certificate APIs), generates its own untrusted
certificate, and installs nothing in Windows certificate stores. Schannel uses a
temporary Windows-managed private key that is removed on certificate disposal.
No external server, firewall change, trusted certificate, or fixed port is needed.
The `tls_integration` CTest entry runs the three handshake tests; these are
separate from the unit-test entries.

For an offline test build, supply an existing checkout of the pinned GoogleTest commit through `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<path>`; the helper accepts extra configure options through `-ConfigureArguments`.

Network tests use the internal `piwik::detail::IHttpTransport` interface and an owning injection constructor on `PiwikDispatcher`. Production uses `WinHttpTransport`; gMock supplies controlled responses while tests exercise real serialization, batching, and request-status handling. Transport headers are not installed as part of the application API. Public tracking methods remain nonvirtual. Tests cover GET/POST request construction, HTTP success/error classification, transport errors, dry runs, batch limits, and per-request outcomes. They use bounded completion checks rather than fixed scheduling sleeps.

The Windows session is opened lazily on the first real send; session/connection/request handles are owned by the transport. HTTP-status-based success classification, no-retry behavior, and the existing shutdown policy are retained. Certificate validation now defaults to enabled, with an explicit per-client opt-out described below. The worker running flag is atomic, and queue/status accesses involved in completion are synchronized. Retry logic, bulk-body validation, and broader shutdown/configuration thread-safety changes remain separate increments.

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

