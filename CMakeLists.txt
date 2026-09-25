# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" SYSTEM_SETTINGS_VERSION)
string(STRIP "${SYSTEM_SETTINGS_VERSION}" SYSTEM_SETTINGS_VERSION)
if(NOT SYSTEM_SETTINGS_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain major.minor.patch")
endif()

project(InfiltratrSystemSettings VERSION "${SYSTEM_SETTINGS_VERSION}" LANGUAGES C)
include(CTest)
include(GNUInstallDirs)

option(SYSTEM_SETTINGS_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
set(SYSTEM_SETTINGS_BUILD_PROFILE "cmake" CACHE STRING
    "Common build-profile identity for this System Settings build")
set_property(CACHE SYSTEM_SETTINGS_BUILD_PROFILE PROPERTY STRINGS
    cmake generic native development)
if(NOT SYSTEM_SETTINGS_BUILD_PROFILE MATCHES "^(cmake|generic|native|development)$")
    message(FATAL_ERROR "Unsupported SYSTEM_SETTINGS_BUILD_PROFILE")
endif()

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
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(SYSTEM_SETTINGS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    endif()
endfunction()

add_library(system-settings-core STATIC
    src/app/project-info.c
    src/modules/date-time/date-time-model.c
    src/modules/date-time/manual-time.c
    src/modules/date-time/native-clock-policy.c)
ss_configure(system-settings-core)
target_compile_definitions(system-settings-core PUBLIC
    SYSTEM_SETTINGS_VERSION="${PROJECT_VERSION}"
    SYSTEM_SETTINGS_BUILD_PROFILE="${SYSTEM_SETTINGS_BUILD_PROFILE}")
target_link_libraries(system-settings-core PUBLIC InfiltratrCommon::Portable)

# Keep the generic shell free of Date & Time domain/back-end ownership.
# The current built-in module uses a private GTK bridge, while the future
# public module ABI remains toolkit-neutral.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/shell/linux/main.c"
    SS_LINUX_SHELL_SOURCE)
foreach(SS_FORBIDDEN_SHELL_TOKEN
        "date-time-model.h"
        "system-time-service.h"
        "location-search.h"
        "regional-context.h"
        "temporal-policy-store.h"
        "SsDateTimeModel"
        "SsSystemTimeService")
    string(FIND "${SS_LINUX_SHELL_SOURCE}"
        "${SS_FORBIDDEN_SHELL_TOKEN}"
        SS_FORBIDDEN_SHELL_INDEX)
    if(NOT SS_FORBIDDEN_SHELL_INDEX EQUAL -1)
        message(FATAL_ERROR
            "Linux shell regained Date & Time domain ownership: "
            "${SS_FORBIDDEN_SHELL_TOKEN}")
    endif()
endforeach()
unset(SS_LINUX_SHELL_SOURCE)

if(WIN32)
    add_library(system-settings-platform STATIC
        src/platform/windows/temporal-policy-store.c)
    target_link_libraries(system-settings-platform
        PUBLIC InfiltratrCommon::Portable shell32 ole32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GIO2 REQUIRED IMPORTED_TARGET gio-2.0)
    pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4>=4.6)
    pkg_check_modules(GEOCODE REQUIRED IMPORTED_TARGET geocode-glib-2.0)

    add_library(system-settings-platform STATIC
        src/platform/linux/cinnamon-interface.c
        src/platform/linux/location-metadata.c
        src/platform/linux/regional-context.c
        src/platform/linux/system-time-service.c
        src/platform/linux/temporal-policy-store.c)
    target_link_libraries(system-settings-platform
        PUBLIC InfiltratrCommon::Common PkgConfig::GIO2)

    add_library(system-settings-linux-ui STATIC
        src/shell/linux/linux-ui-helpers.c)
    ss_configure(system-settings-linux-ui)
    target_include_directories(system-settings-linux-ui PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shell/linux")
    target_link_libraries(system-settings-linux-ui PUBLIC
        PkgConfig::GTK4)

    add_library(system-settings-date-time-linux STATIC
        src/modules/date-time/calendar-preview-provider.c
        src/modules/date-time/linux-date-time-panel.c
        src/modules/date-time/linux-date-time-panel-ui.c
        src/platform/linux/location-search.c)
    ss_configure(system-settings-date-time-linux)
    target_include_directories(system-settings-date-time-linux PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/modules/date-time"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shell/linux")
    target_link_libraries(system-settings-date-time-linux PRIVATE
        system-settings-core
        system-settings-platform
        system-settings-linux-ui
        InfiltratrCommon::Common
        InfiltratrCommon::Portable
        PkgConfig::GTK4
        PkgConfig::GEOCODE
        ${CMAKE_DL_LIBS})
    if(CMAKE_LIBRARY_ARCHITECTURE)
        target_compile_definitions(system-settings-date-time-linux PRIVATE
            SYSTEM_SETTINGS_LIBRARY_ARCHITECTURE="${CMAKE_LIBRARY_ARCHITECTURE}")
    endif()

    add_executable(system-settings
        src/shell/linux/main.c)
    ss_configure(system-settings)
    target_include_directories(system-settings PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/modules/date-time"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shell/linux")
    target_link_libraries(system-settings PRIVATE
        system-settings-date-time-linux
        system-settings-linux-ui
        system-settings-core
        system-settings-platform
        InfiltratrCommon::Portable
        PkgConfig::GTK4)

    install(TARGETS system-settings
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES data/org.infiltrator.SystemSettings.desktop
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES data/org.infiltrator.SystemSettings.svg
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
    install(FILES data/temporal-v3.provider
        DESTINATION "${CMAKE_INSTALL_DATADIR}/infiltrator/policy-providers"
        RENAME temporal-v3)
    install(FILES modules/date-time.settings-module
        DESTINATION "${CMAKE_INSTALL_DATADIR}/infiltrator/system-settings/modules")
else()
    message(FATAL_ERROR "System Settings currently supports Windows policy tools and the Linux GUI.")
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

    add_executable(manual-time-test tests/manual-time-test.c)
    ss_configure(manual-time-test)
    target_link_libraries(manual-time-test PRIVATE system-settings-core)
    add_test(NAME manual-time COMMAND manual-time-test)

    add_executable(native-clock-policy-test
        tests/native-clock-policy-test.c)
    ss_configure(native-clock-policy-test)
    target_link_libraries(native-clock-policy-test PRIVATE
        system-settings-core)
    add_test(NAME native-clock-policy
        COMMAND native-clock-policy-test)

    add_executable(project-info-test tests/project-info-test.c)
    ss_configure(project-info-test)
    target_link_libraries(project-info-test PRIVATE system-settings-core)
    add_test(NAME project-info COMMAND project-info-test)

    if(WIN32)
        add_executable(temporal-policy-store-windows-test
            tests/temporal-policy-store-windows-test.c)
        ss_configure(temporal-policy-store-windows-test)
        target_include_directories(temporal-policy-store-windows-test PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/windows")
        target_link_libraries(temporal-policy-store-windows-test PRIVATE
            system-settings-platform)
        add_test(NAME temporal-policy-store-windows
            COMMAND temporal-policy-store-windows-test)
    endif()

    if(UNIX)
        add_executable(system-time-service-test tests/system-time-service-test.c)
        ss_configure(system-time-service-test)
        target_link_libraries(system-time-service-test PRIVATE system-settings-platform)
        add_test(NAME system-time-service COMMAND system-time-service-test)
        set_tests_properties(system-time-service PROPERTIES TIMEOUT 15)

        add_test(NAME cli-transaction COMMAND ${CMAKE_COMMAND}
            -DCLI=$<TARGET_FILE:system-settings-time>
            -DROOT=${CMAKE_CURRENT_BINARY_DIR}/cli-test-config
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli-transaction-test.cmake)
        add_executable(linux-shell-test tests/linux-shell-test.c)
        ss_configure(linux-shell-test)
        target_include_directories(linux-shell-test PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/modules/date-time"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/shell/linux")
        target_link_libraries(linux-shell-test PRIVATE
            system-settings-date-time-linux system-settings-linux-ui
            system-settings-core system-settings-platform PkgConfig::GTK4)
        find_program(SS_XVFB_RUN xvfb-run)
        if(SS_XVFB_RUN)
            add_test(NAME linux-shell COMMAND ${SS_XVFB_RUN} -a $<TARGET_FILE:linux-shell-test>)
            set_tests_properties(linux-shell PROPERTIES TIMEOUT 30 ENVIRONMENT
                "G_DEBUG=fatal-criticals;GTK_A11Y=none;GSK_RENDERER=cairo;GSETTINGS_BACKEND=memory")
        else()
            message(WARNING "xvfb-run missing: linux-shell regression is not registered")
        endif()
        add_executable(temporal-policy-store-test
            tests/temporal-policy-store-test.c)
        ss_configure(temporal-policy-store-test)
        target_link_libraries(temporal-policy-store-test PRIVATE
            system-settings-platform)
        add_test(NAME temporal-policy-store COMMAND temporal-policy-store-test)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/empty-schemas")
        set_tests_properties(temporal-policy-store PROPERTIES ENVIRONMENT
            "GSETTINGS_BACKEND=memory;GSETTINGS_SCHEMA_DIR=${CMAKE_CURRENT_BINARY_DIR}/empty-schemas;XDG_DATA_DIRS=${CMAKE_CURRENT_BINARY_DIR}/empty-schemas")

        add_executable(regional-context-test
            tests/regional-context-test.c)
        ss_configure(regional-context-test)
        target_link_libraries(regional-context-test PRIVATE
            system-settings-platform)
        add_test(NAME regional-context COMMAND regional-context-test)

        add_executable(location-metadata-test
            tests/location-metadata-test.c)
        ss_configure(location-metadata-test)
        target_link_libraries(location-metadata-test PRIVATE
            system-settings-platform)
        add_test(NAME location-metadata COMMAND location-metadata-test)

        add_library(calendar-preview-runtime-fixture SHARED
            tests/calendar-preview-runtime-fixture.c)
        ss_configure(calendar-preview-runtime-fixture)
        target_link_libraries(calendar-preview-runtime-fixture PRIVATE
            PkgConfig::GIO2)
        set_target_properties(calendar-preview-runtime-fixture PROPERTIES
            PREFIX ""
            OUTPUT_NAME "calendar-preview-runtime-fixture")

        add_executable(calendar-preview-provider-test
            tests/calendar-preview-provider-test.c
            src/modules/date-time/calendar-preview-provider.c)
        ss_configure(calendar-preview-provider-test)
        target_include_directories(calendar-preview-provider-test PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/modules/date-time")
        target_link_libraries(calendar-preview-provider-test PRIVATE
            InfiltratrCommon::Common
            PkgConfig::GIO2
            ${CMAKE_DL_LIBS})
        if(CMAKE_LIBRARY_ARCHITECTURE)
            target_compile_definitions(calendar-preview-provider-test PRIVATE
                SYSTEM_SETTINGS_LIBRARY_ARCHITECTURE="${CMAKE_LIBRARY_ARCHITECTURE}")
        endif()
        add_test(NAME calendar-preview-provider
            COMMAND calendar-preview-provider-test
                $<TARGET_FILE:calendar-preview-runtime-fixture>)
    endif()
endif()


install(TARGETS system-settings-time
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")


set(CPACK_PACKAGE_NAME "infiltrator-system-settings")
set(CPACK_PACKAGE_VENDOR "Infiltrator Projects")
set(CPACK_PACKAGE_CONTACT "Shannon Smith <infiltratr@yandex.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Native system-wide settings shell for Infiltrator applications")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")

if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()

include(CPack)
