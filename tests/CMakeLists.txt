add_executable(sdk_tests serialization_tests.cpp encoding_tests.cpp query_encoding_tests.cpp numeric_tests.cpp dimension_tests.cpp client_tests.cpp dispatcher_tests.cpp)
target_link_libraries(sdk_tests PRIVATE Matomo::SDK GTest::gmock_main)
target_compile_options(sdk_tests PRIVATE /utf-8 /EHsc)
include(GoogleTest)
gtest_discover_tests(sdk_tests DISCOVERY_TIMEOUT 30 PROPERTIES TIMEOUT 15)

# Real WinHTTP handshakes against an isolated loopback TLS fixture. No Matomo
# server, persistent certificate, or certificate-store changes are required.
find_program(PIWIK_TEST_POWERSHELL NAMES pwsh powershell REQUIRED)
add_executable(tls_integration_tests tls/live_tls_tests.cpp)
target_link_libraries(tls_integration_tests PRIVATE Matomo::SDK GTest::gtest_main)
target_compile_options(tls_integration_tests PRIVATE /utf-8 /EHsc)
add_test(NAME tls_integration
    COMMAND "${PIWIK_TEST_POWERSHELL}" -NoLogo -NoProfile -NonInteractive
        -File "${CMAKE_CURRENT_SOURCE_DIR}/tls/run_tls_tests.ps1"
        -TestExecutable "$<TARGET_FILE:tls_integration_tests>")
set_tests_properties(tls_integration PROPERTIES TIMEOUT 90)
