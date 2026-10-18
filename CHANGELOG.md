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

This extraction retains the existing certificate bypass, response-status policy,
no-retry behavior, and shutdown policy. Performance and structured-error features
remain planned work. Installed static-library consumers must rebuild against the
updated headers and library together.
