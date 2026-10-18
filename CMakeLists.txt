cmake_minimum_required(VERSION 3.21)
project(PiwikClient VERSION 1.1.0 LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC OR MSVC_VERSION LESS 1920)
    message(FATAL_ERROR "The SDK requires Windows and MSVC from Visual Studio 2019 or newer.")
endif()
if(DEFINED PIWIK_EXPECTED_POINTER_SIZE AND
   NOT CMAKE_SIZEOF_VOID_P EQUAL PIWIK_EXPECTED_POINTER_SIZE)
    message(FATAL_ERROR "Compiler architecture does not match the preset. Use the matching MSVC developer environment or tools/build.ps1.")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(PIWIK_UNICODE "Build the Unicode API (must match the consuming application)" ON)
set(PIWIK_WINDOWS_API_VERSION "0x0601" CACHE STRING "Windows API baseline; XP is unsupported")
if(PIWIK_WINDOWS_API_VERSION LESS 0x0601)
    message(FATAL_ERROR "The Windows API baseline must be at least 0x0601; Windows XP is unsupported.")
endif()

add_library(PiwikClient STATIC
    src/Client.cpp
    src/Dispatcher.cpp
    src/State.cpp
    src/Utilities.cpp
)
add_library(Matomo::SDK ALIAS PiwikClient)
set_target_properties(PiwikClient PROPERTIES EXPORT_NAME SDK DEBUG_POSTFIX d)
target_compile_features(PiwikClient PUBLIC cxx_std_14)
target_compile_options(PiwikClient PRIVATE /W3 /utf-8 /EHsc)
target_compile_definitions(PiwikClient PUBLIC _WIN32_WINNT=${PIWIK_WINDOWS_API_VERSION})
if(PIWIK_UNICODE)
    target_compile_definitions(PiwikClient PUBLIC UNICODE _UNICODE)
endif()
target_include_directories(PiwikClient PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include/matomo>
)
target_link_libraries(PiwikClient PUBLIC winhttp advapi32 user32)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS PiwikClient EXPORT MatomoSDKTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/Piwik.h
    src/Config.h src/Utilities.h src/State.h src/Dispatcher.h src/Client.h
    DESTINATION include/matomo)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/MatomoSDK)
install(EXPORT MatomoSDKTargets FILE MatomoSDKTargets.cmake NAMESPACE Matomo::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MatomoSDK)
configure_package_config_file(cmake/MatomoSDKConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/MatomoSDKConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MatomoSDK)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/MatomoSDKConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/MatomoSDKConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/MatomoSDKConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MatomoSDK)

# A consumer adding the SDK as a subdirectory does not build its tests by default.
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    include(CTest)
else()
    option(BUILD_TESTING "Build tests" OFF)
endif()
if(BUILD_TESTING)
    include(FetchContent)
    # GoogleTest 1.14 supports C++14. Pin the release commit, not a moving branch.
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
    if(CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL")
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    else()
        set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
    endif()
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG f8d7d77c06936315286eb55f8de22cd23c188571
    )
    FetchContent_MakeAvailable(googletest)
    add_subdirectory(tests)
endif()
