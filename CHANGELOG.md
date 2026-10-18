# Changelog

## Unreleased

### GET and bulk query encoding

- URL-encode every string value in both GET and bulk requests, preserving query
  delimiters, plus signs, literal percent escapes, quotes, controls, and UTF-8.
- JSON-escape custom variable names/values, then URL-encode the entire JSON
  collection once; preserve sparse one-based slots for visit and action variables.
- Restrict unescaped percent-encoder bytes to ASCII unreserved characters instead
  of using locale-dependent character classification.
- Return the same encoded query from both state serialization formats; the
  dispatcher still supplies the outer bulk JSON object. No tracking signatures
  or parameter names change. Special-character values may now appear correctly
  in reports where previous requests were corrupted or rejected.
- Add decoded round-trip tests for all string fields, custom-variable JSON,
  Unicode, every byte, and captured multi-event bulk bodies; extend the installed
  static-library consumer. Live Matomo ingestion/reporting is not tested locally.

### Per-event custom dimensions

- Add owned `PiwikActionDimensions` with numeric IDs 1..999, replacement, removal,
  clearing, and read-only inspection. Attach it through `PiwikState::ActionDimensions`
  for application-defined outcome/error context without shared visit-state mutation.
- Snapshot dimensions when queuing, retain full strings without legacy slot/length
  truncation, and support explicit empty values. Server configuration/limits apply.
- Fix legacy dimension serialization: remove literal wrapper quotes and URL-encode
  dimension names/values in both GET and bulk requests. Corrected values may appear
  separately from historical quoted report values.
- Emit identical wire keys once, preferring typed action values over legacy action
  values over visit defaults. Last occurrence wins within each legacy array.
- Add collection, encoding, Unicode, precedence, per-event isolation, queue snapshot,
  legacy client, and installed-package regression coverage. Rebuild headers and the
  static library together because the tracking-state layout changed.

### Numeric measurement correctness and legacy timing

- Serialize numbers using the classic locale with round-trip floating precision;
  encode positive exponent signs in both GET and bulk query strings.
- Store event values as double. Add explicit presence via `SetEventValue(double)`
  and `ClearEventValue()` so zero can be measured without changing the existing
  `TrackEvent(..., float val = 0)` omission behavior.
- Reject NaN and infinity for events/revenue before queuing. Direct serialization
  of invalid numeric states throws `std::invalid_argument`.
- Mark `gt_ms`, `AmountOfTime`, and legacy timing arguments as legacy-only in
  source and documentation. Matomo 4+ no longer records generation time. Preserve
  older-server wire behavior; recommend named event values for app durations.
- Add locale, precision, exponent, explicit-zero, invalid-number, client/dispatcher,
  and legacy-timing regression coverage. Static consumers must rebuild headers
  and library together because the tracking-state layout changed.

- Replace maintained VS2010 solutions/projects with CMake 3.21+ and Ninja presets.
- Require MSVC from Visual Studio 2019 or newer and retain C++14 compatibility.
- Build an installable static library with a `Matomo::SDK` CMake package target.
- Provide x86/x64 Debug/Release presets, defaulting to Unicode and static CRT.
- Remove the XP API target; default to the Windows 7 API surface, with runtime
  validation limited to development and CI operating systems.
- Add pinned GoogleTest/gMock 1.14.0, serialization/encoding/client tests, and a
  Windows CI matrix using the v142 compiler toolset.
- Add a PowerShell helper for initializing MSVC and running the presets.

### Internal HTTP transport and delivery tests

- Extract WinHTTP behind `piwik::detail::IHttpTransport`, with owned transport
  injection at the dispatcher and RAII ownership of WinHTTP handles.
- Open the HTTP session on first send, allowing mock-only tests and dry runs
  without initializing WinHTTP.
- Add gMock delivery tests for GET/POST construction, HTTP and network errors,
  dry runs, batched outcomes, bundle limits, and failure followed by success.
- Synchronize queue emptiness and acknowledged status, and make the worker
  running flag atomic so completion checks do not race those fields.

The transport extraction retained the existing response-status, no-retry, and
shutdown policies. Performance and structured-error features remain planned work.
Installed static-library consumers must rebuild against the updated headers and
library together.

### Configurable server certificate validation

- Enable HTTPS certificate validation by default (a behavior change from the old
  unconditional certificate bypass).
- Add `SetVerifyServerCertificate(bool)` and `VerifiesServerCertificate()` to the
  public client API, backed by an atomic per-client dispatcher setting.
- Support an explicit opt-out using the four legacy ignore flags; log a warning
  to the configured logger when validation is disabled.
- Keep HTTP unaffected by certificate flags and never automatically bypass a
  certificate failure. Recreate the WinHTTP session when verification policy changes.
- Test defaults, opt-out, restoration, client isolation, warning logging, Windows
  flag selection, and secure-failure delivery. Extend the installed-package check
  to exercise the new public methods.
- Add three real WinHTTP TLS handshake checks against a temporary loopback-only
  server, included in CTest/CI. The fixture uses an untrusted self-signed
  certificate without installing certificates in Windows stores.
- Allow an explicit port on internal transport requests for the randomly assigned
  loopback fixture port; production dispatch continues using its existing default.
