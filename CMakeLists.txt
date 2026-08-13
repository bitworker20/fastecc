# === FourQ库配置 ===
# 由于FourQ是独立的C库，保留基本项目信息但移除重复配置
cmake_minimum_required(VERSION 3.20)

project(fastecc 
    VERSION 0.1.0
    DESCRIPTION "Fast Elliptic Curve Cryptography wrapper for FourQ"
)

# Option to enable/disable building tests (needed for standalone testing)
option(BUILD_TESTING "Build the tests" ON)

# Set C++ standard (replicating parent settings)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compiler warnings (replicating parent settings)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wformat=2
        -Wconversion
        -Wnull-dereference
        -Wno-unused-variable
    )
elseif(MSVC)
    add_compile_options(
        /W4
        /WX
        /permissive-
    )
endif()

# --- FourQ Library (libfourq) ---
add_library(fourq STATIC
    # C++ Wrapper Implementation
    fourq.cpp
    # Add FourQ C source files directly to the library target
    FourQlib/FourQ_64bit_and_portable/eccp2_core.c
    FourQlib/FourQ_64bit_and_portable/eccp2_no_endo.c
    FourQlib/FourQ_64bit_and_portable/crypto_util.c
    FourQlib/FourQ_64bit_and_portable/schnorrq.c
    FourQlib/sha512/sha512.c
    FourQlib/random/random.c
)

# 使用父项目的通用配置
#configure_target(fourq)

# Set include directories
target_include_directories(fourq PUBLIC
    # C++ headers (fourq.hpp, utils.hpp)
    ${CMAKE_CURRENT_SOURCE_DIR}
    # FourQ C headers (FourQ.h, FourQ_api.h, FourQ_internal.h etc.)
    ${CMAKE_CURRENT_SOURCE_DIR}/FourQlib/FourQ_64bit_and_portable
)

# Add compile definitions for USE_ENDO
# --- DISABLED --- 
# target_compile_definitions(fourq PUBLIC USE_ENDO=true)

# Set platform-specific compile definitions
# CPU first (picks AMD64/fp_x64.h vs ARM64/fp_arm64.h in FourQ.h), OS second
# (picks the random_bytes() backend in FourQlib/random/random.c). The two are
# independent; folding them together used to force _ARM64_ on every Apple host,
# which defined both _AMD64_ and _ARM64_ on an Intel Mac.
# Apple reports its 64-bit ARM as "arm64", not "aarch64".
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    target_compile_definitions(fourq PUBLIC _AMD64_)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    target_compile_definitions(fourq PUBLIC _ARM64_)
else()
    message(FATAL_ERROR "fastecc: unsupported CPU '${CMAKE_SYSTEM_PROCESSOR}' (need x86_64 or arm64)")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux|Android|Darwin")
    # Android is bionic/Linux, and Darwin is close enough, as far as FourQ.h's
    # OS gate is concerned: it only selects the POSIX open("/dev/urandom")
    # branch over the Windows BCrypt one.
    target_compile_definitions(fourq PUBLIC __LINUX__)
endif()

# Suppress warnings for FourQ C sources
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message("Set compiler flags")
    set_source_files_properties(
        FourQlib/FourQ_64bit_and_portable/eccp2_core.c
        FourQlib/FourQ_64bit_and_portable/eccp2_no_endo.c
        FourQlib/FourQ_64bit_and_portable/crypto_util.c
	    FourQlib/FourQ_64bit_and_portable/schnorrq.c
	    #schnorrq_new.c
        FourQlib/sha512/sha512.c
        FourQlib/random/random.c
        PROPERTIES COMPILE_FLAGS "-Wno-uninitialized -Wno-conversion -Wno-unused-variable"
    )
endif()

# === WORKAROUND for FourQ optimization issues ===
# 强制使用 -O1 优化级别以避免FourQ运行时错误
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    message(STATUS "Fastecc: Lowering optimization to -O1 for 'fourq' target in ${CMAKE_BUILD_TYPE} build.")
    set_property(TARGET fourq APPEND_STRING PROPERTY COMPILE_FLAGS " -O1 ") 
endif()

# === 测试配置 ===
if(BUILD_TESTING)
    # Add test executable
    #add_executable(run_fourq_tests test_fourq.cpp)
    
    # 使用通用配置
    #configure_target(run_fourq_tests)

    # Link against dependencies
    #target_link_libraries(run_fourq_tests PRIVATE 
    #    fourq
    #    GTest::gtest_main
    #)

    # Configure automatic test discovery
    #gtest_discover_tests(run_fourq_tests)
endif()
