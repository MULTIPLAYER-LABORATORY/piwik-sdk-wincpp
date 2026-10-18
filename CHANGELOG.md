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

This increment does not change tracking semantics or add the proposed performance,
structured-error, HTTP transport injection, or delivery-reliability features.
