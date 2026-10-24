# Run with cmake -DSDK_BUILD_DIR=<absolute build directory> -P verify.cmake
# from the same MSVC developer environment used for the SDK build.
cmake_minimum_required(VERSION 3.20)
if(NOT DEFINED SDK_BUILD_DIR)
    message(FATAL_ERROR "SDK_BUILD_DIR is required")
endif()
get_filename_component(SDK_BUILD_DIR "${SDK_BUILD_DIR}" ABSOLUTE)
set(package_prefix "${SDK_BUILD_DIR}/package-check/install")
set(consumer_build "${SDK_BUILD_DIR}/package-check/consumer")
load_cache("${SDK_BUILD_DIR}" READ_WITH_PREFIX sdk_
    CMAKE_BUILD_TYPE CMAKE_MSVC_RUNTIME_LIBRARY CMAKE_CXX_COMPILER)
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${SDK_BUILD_DIR}"
    --prefix "${package_prefix}" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${CMAKE_COMMAND}"
    -S "${CMAKE_CURRENT_LIST_DIR}" -B "${consumer_build}" -G Ninja
    "-DCMAKE_PREFIX_PATH=${package_prefix}"
    "-DCMAKE_BUILD_TYPE=${sdk_CMAKE_BUILD_TYPE}"
    "-DCMAKE_CXX_COMPILER=${sdk_CMAKE_CXX_COMPILER}"
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=${sdk_CMAKE_MSVC_RUNTIME_LIBRARY}"
    COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${consumer_build}"
    COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${consumer_build}/package_consumer.exe"
    COMMAND_ERROR_IS_FATAL ANY)
