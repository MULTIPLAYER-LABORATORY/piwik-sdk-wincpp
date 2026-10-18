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

GoogleTest/gMock 1.14.0 is pinned by commit and built in C++14 mode with the same runtime as the SDK. The initial tests cover encoding, Unicode conversion, event serialization, visit dimensions, optional fields, and linking/configuring the real client. The helper and CI also install a package into an isolated directory, build a standalone consumer with `find_package`, and run it. Tests do not contact Matomo or write visitor registry state. gMock is available for the next increment; no public tracking methods have been made virtual.

For an offline test build, supply an existing checkout of the pinned GoogleTest commit through `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<path>`; the helper accepts extra configure options through `-ConfigureArguments`.

Future network tests should introduce a small internal HTTP transport interface so requests and responses can be substituted without replacing the client or serializer. Timing dependencies can be injected separately when session/retry behavior is tested. Known legacy behaviors such as omission of zero event values and action-dimension wire formatting should receive dedicated regression tests with their fixes rather than being declared correct by this migration.

GitHub Actions builds and tests x86/x64 Debug/Release using the VS2019 v142 toolset. CMake presets replace the old checked-in Visual Studio 2010 project files as the maintained build entry point. The historical Windows GUI sample source remains for reference.

After instantiating the PiwikClient object, which is the main component providing tracking facilities to the application, a set of properties can be assigned to it in order to define the appropriate endpoints and customize its behavior. Different operation modes are selectable like using synchronous or asynchronous dispatching with regulable time frames, sending HTTP requests with GET or POST methods or using encrypted secure connections. A stream pointer can also be passed to the tracker to activate logging facilities. 

Direct entry points are provided to the hosting application to track standard targets in a convenient way: user events, screen views, goals, display or content interactions can be tracked by simply supplying the most relevant parameters for each one of them. For more advanced usages, a low-level call is also provided in which the whole set of trackable values is made accessible to the calling function thus allowing to track complex or non-standard situations. At any time pending events can be flushed to the server if required and tracking can be finished by simply deleting the PiwikClient instance.

### Interface

To get a detailed insight into the interface of the library take a look at the [Reference Documentation](doc/Reference.md).

