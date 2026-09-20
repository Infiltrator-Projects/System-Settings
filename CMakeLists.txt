# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)
project(InfiltratrSystemSettings VERSION 0.1.0 LANGUAGES C)
include(CTest)

option(SYSTEM_SETTINGS_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/infiltratr-common/CMakeLists.txt")
    message(FATAL_ERROR "Pinned Infiltratr Common submodule is required.")
endif()
add_subdirectory(src/vendor/infiltratr-common common-build EXCLUDE_FROM_ALL)

set(SS_WARNING_FLAGS -Wall -Wextra -Wpedantic -Wshadow -Wformat=2
    -Wstrict-prototypes -Wmissing-prototypes)

function(ss_configure target)
    target_compile_features(${target} PRIVATE c_std_11)
    target_include_directories(${target} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${target} PRIVATE ${SS_WARNING_FLAGS})
        if(SYSTEM_SETTINGS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(system-settings-core STATIC
    src/modules/date-time/date-time-model.c)
ss_configure(system-settings-core)
target_link_libraries(system-settings-core PUBLIC InfiltratrCommon::Portable)

if(WIN32)
    add_library(system-settings-platform STATIC
        src/platform/windows/temporal-policy-store.c)
    target_link_libraries(system-settings-platform
        PUBLIC InfiltratrCommon::Portable shell32 ole32)
elseif(UNIX)
    add_library(system-settings-platform STATIC
        src/platform/linux/temporal-policy-store.c)
    target_link_libraries(system-settings-platform
        PUBLIC InfiltratrCommon::Common)
else()
    message(FATAL_ERROR "System Settings currently supports Windows and POSIX/Linux.")
endif()
ss_configure(system-settings-platform)

add_executable(system-settings-time src/tools/time-settings-cli.c)
ss_configure(system-settings-time)
target_link_libraries(system-settings-time PRIVATE system-settings-core system-settings-platform)

if(BUILD_TESTING)
    add_executable(date-time-model-test tests/date-time-model-test.c)
    ss_configure(date-time-model-test)
    target_link_libraries(date-time-model-test PRIVATE system-settings-core)
    add_test(NAME date-time-model COMMAND date-time-model-test)
endif()
