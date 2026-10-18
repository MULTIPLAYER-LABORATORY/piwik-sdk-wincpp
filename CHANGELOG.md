# Changelog

## Unreleased

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
