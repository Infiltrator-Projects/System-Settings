# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" SYSTEM_SETTINGS_VERSION)
string(STRIP "${SYSTEM_SETTINGS_VERSION}" SYSTEM_SETTINGS_VERSION)
if(NOT SYSTEM_SETTINGS_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain major.minor.patch")
endif()

project(InfiltratrSystemSettings VERSION "${SYSTEM_SETTINGS_VERSION}" LANGUAGES C)
include(CTest)

# System Settings is a system control panel. Keep the runtime paths compiled
# into the executable identical to the paths used by the Debian package.
# CPack's DEB staging is under /usr; leaving CMake's generic /usr/local default
# here made packaged builds look for UI bitmaps in /usr/local/share while the
# package installed them in /usr/share.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
   CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH
        "Install prefix for the Linux system settings shell" FORCE)
endif()
include(GNUInstallDirs)

option(SYSTEM_SETTINGS_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
set(SYSTEM_SETTINGS_BUILD_PROFILE "cmake" CACHE STRING
    "Common build-profile identity for this System Settings build")
set_property(CACHE SYSTEM_SETTINGS_BUILD_PROFILE PROPERTY STRINGS
    cmake generic native development)
set(SYSTEM_SETTINGS_NATIVE_PROFILE_DATA_DIR "" CACHE PATH
    "Directory containing trained GCC PGO data for native builds")
if(NOT SYSTEM_SETTINGS_BUILD_PROFILE MATCHES "^(cmake|generic|native|development)$")
    message(FATAL_ERROR "Unsupported SYSTEM_SETTINGS_BUILD_PROFILE")
endif()

if(SYSTEM_SETTINGS_BUILD_PROFILE STREQUAL "native")
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR
            "The native build profile requires GCC so trained PGO identity is truthful.")
    endif()
    string(TOUPPER "${CMAKE_BUILD_TYPE}" SS_BUILD_TYPE_UPPER)
    set(SS_BUILD_TYPE_FLAGS_VAR "CMAKE_C_FLAGS_${SS_BUILD_TYPE_UPPER}")
    set(SS_NATIVE_PROFILE_FLAGS
        "${CMAKE_C_FLAGS} ${${SS_BUILD_TYPE_FLAGS_VAR}}")
    if(NOT SS_NATIVE_PROFILE_FLAGS MATCHES "(^|[ ;])-march=native([ ;]|$)" OR
       NOT SS_NATIVE_PROFILE_FLAGS MATCHES "(^|[ ;])-flto([= ;]|$)" OR
       NOT SS_NATIVE_PROFILE_FLAGS MATCHES "(^|[ ;])-fprofile-use(=|[ ;])")
        message(FATAL_ERROR
            "SYSTEM_SETTINGS_BUILD_PROFILE=native requires -march=native, LTO and a trained -fprofile-use build.")
    endif()
    if(SYSTEM_SETTINGS_NATIVE_PROFILE_DATA_DIR STREQUAL "" OR
       NOT IS_DIRECTORY "${SYSTEM_SETTINGS_NATIVE_PROFILE_DATA_DIR}")
        message(FATAL_ERROR
            "Native identity requires the trained GCC PGO data directory.")
    endif()
    file(GLOB_RECURSE SS_NATIVE_PROFILE_DATA
        "${SYSTEM_SETTINGS_NATIVE_PROFILE_DATA_DIR}/*.gcda")
    if(NOT SS_NATIVE_PROFILE_DATA)
        message(FATAL_ERROR
            "Native identity requires actual GCC training data (*.gcda).")
    endif()
    unset(SS_NATIVE_PROFILE_DATA)
    unset(SS_NATIVE_PROFILE_FLAGS)
    unset(SS_BUILD_TYPE_FLAGS_VAR)
    unset(SS_BUILD_TYPE_UPPER)
endif()

set(SYSTEM_SETTINGS_COMMON_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/infiltratr-common")
set(SYSTEM_SETTINGS_COMMON_EXPECTED_VERSION "1.19.38")
set(SYSTEM_SETTINGS_COMMON_EXPECTED_COMMIT
    "d01a93153940847cee39ac6ce5ebc42117c25b78")

if(NOT EXISTS "${SYSTEM_SETTINGS_COMMON_DIR}/CMakeLists.txt" OR
   NOT EXISTS "${SYSTEM_SETTINGS_COMMON_DIR}/VERSION")
    message(FATAL_ERROR "Pinned Infiltratr Common submodule is required.")
endif()
file(READ "${SYSTEM_SETTINGS_COMMON_DIR}/VERSION"
    SYSTEM_SETTINGS_COMMON_VERSION)
string(STRIP "${SYSTEM_SETTINGS_COMMON_VERSION}"
    SYSTEM_SETTINGS_COMMON_VERSION)
if(NOT SYSTEM_SETTINGS_COMMON_VERSION STREQUAL
   SYSTEM_SETTINGS_COMMON_EXPECTED_VERSION)
    message(FATAL_ERROR
        "System Settings requires Common "
        "${SYSTEM_SETTINGS_COMMON_EXPECTED_VERSION}; found "
        "${SYSTEM_SETTINGS_COMMON_VERSION}")
endif()
find_package(Git QUIET)
set(SYSTEM_SETTINGS_COMMON_PROVENANCE
    "${SYSTEM_SETTINGS_COMMON_DIR}/.system-settings-common-commit")
if(GIT_FOUND AND EXISTS "${SYSTEM_SETTINGS_COMMON_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${SYSTEM_SETTINGS_COMMON_DIR}"
                rev-parse HEAD
        RESULT_VARIABLE SYSTEM_SETTINGS_COMMON_GIT_RESULT
        OUTPUT_VARIABLE SYSTEM_SETTINGS_COMMON_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(NOT SYSTEM_SETTINGS_COMMON_GIT_RESULT EQUAL 0 OR
       NOT SYSTEM_SETTINGS_COMMON_COMMIT STREQUAL
           SYSTEM_SETTINGS_COMMON_EXPECTED_COMMIT)
        message(FATAL_ERROR
            "System Settings requires Common commit "
            "${SYSTEM_SETTINGS_COMMON_EXPECTED_COMMIT}; found "
            "${SYSTEM_SETTINGS_COMMON_COMMIT}")
    endif()
elseif(EXISTS "${SYSTEM_SETTINGS_COMMON_PROVENANCE}")
    file(READ "${SYSTEM_SETTINGS_COMMON_PROVENANCE}"
        SYSTEM_SETTINGS_COMMON_COMMIT)
    string(STRIP "${SYSTEM_SETTINGS_COMMON_COMMIT}"
        SYSTEM_SETTINGS_COMMON_COMMIT)
    if(NOT SYSTEM_SETTINGS_COMMON_COMMIT STREQUAL
       SYSTEM_SETTINGS_COMMON_EXPECTED_COMMIT)
        message(FATAL_ERROR
            "Exported Common provenance does not match required commit "
            "${SYSTEM_SETTINGS_COMMON_EXPECTED_COMMIT}; found "
            "${SYSTEM_SETTINGS_COMMON_COMMIT}")
    endif()

    set(SYSTEM_SETTINGS_COMMON_HASH_MANIFEST
        "${SYSTEM_SETTINGS_COMMON_DIR}/.system-settings-common-sha256")
    if(NOT EXISTS "${SYSTEM_SETTINGS_COMMON_HASH_MANIFEST}")
        message(FATAL_ERROR
            "Exported Common source is missing its byte-integrity manifest.")
    endif()
    file(STRINGS "${SYSTEM_SETTINGS_COMMON_HASH_MANIFEST}"
        SYSTEM_SETTINGS_COMMON_HASH_LINES)
    set(SYSTEM_SETTINGS_COMMON_MANIFEST_PATHS)
    foreach(SYSTEM_SETTINGS_COMMON_HASH_LINE
            IN LISTS SYSTEM_SETTINGS_COMMON_HASH_LINES)
        string(LENGTH "${SYSTEM_SETTINGS_COMMON_HASH_LINE}"
            SYSTEM_SETTINGS_COMMON_HASH_LINE_LENGTH)
        if(SYSTEM_SETTINGS_COMMON_HASH_LINE_LENGTH LESS 67)
            message(FATAL_ERROR
                "Malformed Common byte-integrity manifest entry.")
        endif()
        string(SUBSTRING "${SYSTEM_SETTINGS_COMMON_HASH_LINE}" 0 64
            SYSTEM_SETTINGS_COMMON_FILE_HASH)
        string(SUBSTRING "${SYSTEM_SETTINGS_COMMON_HASH_LINE}" 64 2
            SYSTEM_SETTINGS_COMMON_HASH_SEPARATOR)
        string(SUBSTRING "${SYSTEM_SETTINGS_COMMON_HASH_LINE}" 66 -1
            SYSTEM_SETTINGS_COMMON_RELATIVE_PATH)
        if(NOT SYSTEM_SETTINGS_COMMON_FILE_HASH MATCHES "^[0-9a-fA-F]+$" OR
           NOT SYSTEM_SETTINGS_COMMON_HASH_SEPARATOR STREQUAL "  " OR
           SYSTEM_SETTINGS_COMMON_RELATIVE_PATH STREQUAL "" OR
           SYSTEM_SETTINGS_COMMON_RELATIVE_PATH MATCHES "(^|/)\\.\\.(/|$)")
            message(FATAL_ERROR
                "Malformed Common byte-integrity manifest entry.")
        endif()
        set(SYSTEM_SETTINGS_COMMON_FILE
            "${SYSTEM_SETTINGS_COMMON_DIR}/${SYSTEM_SETTINGS_COMMON_RELATIVE_PATH}")
        if(NOT EXISTS "${SYSTEM_SETTINGS_COMMON_FILE}" OR
           IS_DIRECTORY "${SYSTEM_SETTINGS_COMMON_FILE}")
            message(FATAL_ERROR
                "Common manifest references missing file: "
                "${SYSTEM_SETTINGS_COMMON_RELATIVE_PATH}")
        endif()
        file(SHA256 "${SYSTEM_SETTINGS_COMMON_FILE}"
            SYSTEM_SETTINGS_COMMON_ACTUAL_HASH)
        if(NOT SYSTEM_SETTINGS_COMMON_ACTUAL_HASH STREQUAL
           SYSTEM_SETTINGS_COMMON_FILE_HASH)
            message(FATAL_ERROR
                "Common source byte mismatch: "
                "${SYSTEM_SETTINGS_COMMON_RELATIVE_PATH}")
        endif()
        list(APPEND SYSTEM_SETTINGS_COMMON_MANIFEST_PATHS
            "${SYSTEM_SETTINGS_COMMON_RELATIVE_PATH}")
    endforeach()
    file(GLOB_RECURSE SYSTEM_SETTINGS_COMMON_EXPORTED_FILES
        RELATIVE "${SYSTEM_SETTINGS_COMMON_DIR}"
        "${SYSTEM_SETTINGS_COMMON_DIR}/*")
    list(REMOVE_ITEM SYSTEM_SETTINGS_COMMON_EXPORTED_FILES
        ".system-settings-common-commit"
        ".system-settings-common-sha256")
    list(SORT SYSTEM_SETTINGS_COMMON_EXPORTED_FILES)
    list(SORT SYSTEM_SETTINGS_COMMON_MANIFEST_PATHS)
    if(NOT SYSTEM_SETTINGS_COMMON_EXPORTED_FILES STREQUAL
       SYSTEM_SETTINGS_COMMON_MANIFEST_PATHS)
        message(FATAL_ERROR
            "Exported Common source contains files not covered by its byte-integrity manifest.")
    endif()
else()
    message(FATAL_ERROR
        "Exact Common provenance is required. Use the pinned Git submodule "
        "or an official System Settings source archive.")
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
file(GLOB SS_LINUX_SHELL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/shell/linux/*.c")
foreach(SS_LINUX_SHELL_FILE IN LISTS SS_LINUX_SHELL_SOURCES)
    file(READ "${SS_LINUX_SHELL_FILE}" SS_LINUX_SHELL_SOURCE)
    foreach(SS_FORBIDDEN_SHELL_TOKEN
            "date-time-model.h"
            "system-time-service.h"
            "location-search.h"
            "location-metadata.h"
            "regional-context.h"
            "temporal-policy-store.h"
            "SsDateTimeModel"
            "SsSystemTimeService")
        string(FIND "${SS_LINUX_SHELL_SOURCE}"
            "${SS_FORBIDDEN_SHELL_TOKEN}"
            SS_FORBIDDEN_SHELL_INDEX)
        if(NOT SS_FORBIDDEN_SHELL_INDEX EQUAL -1)
            message(FATAL_ERROR
                "Linux shell regained Date & Time domain ownership in "
                "${SS_LINUX_SHELL_FILE}: ${SS_FORBIDDEN_SHELL_TOKEN}")
        endif()
    endforeach()
endforeach()
unset(SS_LINUX_SHELL_SOURCE)
unset(SS_LINUX_SHELL_SOURCES)

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
        src/shell/linux/linux-ui-helpers.c
        src/shell/linux/linux-theme.c)
    ss_configure(system-settings-linux-ui)
    target_include_directories(system-settings-linux-ui PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shell/linux")
    target_link_libraries(system-settings-linux-ui PUBLIC
        PkgConfig::GTK4
        InfiltratrCommon::Portable)

    add_library(system-settings-date-time-linux STATIC
        src/modules/date-time/calendar-preview-provider.c
        src/modules/date-time/home-temporal-presentation.c
        src/modules/date-time/policy-file-observer.c
        src/modules/date-time/linux-date-time-panel.c
        src/modules/date-time/linux-date-time-location.c
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

    set(SYSTEM_SETTINGS_RUNTIME_UI_SOURCE_DIR "")
    set(SYSTEM_SETTINGS_RUNTIME_MODULE_SOURCE_DIR "")
    if(SYSTEM_SETTINGS_BUILD_PROFILE STREQUAL "cmake" OR
       SYSTEM_SETTINGS_BUILD_PROFILE STREQUAL "development")
        set(SYSTEM_SETTINGS_RUNTIME_UI_SOURCE_DIR
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui")
        set(SYSTEM_SETTINGS_RUNTIME_MODULE_SOURCE_DIR
            "${CMAKE_CURRENT_SOURCE_DIR}/modules")
    endif()
    target_include_directories(system-settings PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/modules/date-time"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shell/linux")
    target_compile_definitions(system-settings PRIVATE
        SYSTEM_SETTINGS_UI_ASSET_DIR="${CMAKE_INSTALL_FULL_DATADIR}/infiltrator/system-settings/ui"
        SYSTEM_SETTINGS_UI_SOURCE_DIR="${SYSTEM_SETTINGS_RUNTIME_UI_SOURCE_DIR}"
        SYSTEM_SETTINGS_MODULE_DIR="${CMAKE_INSTALL_FULL_DATADIR}/infiltrator/system-settings/modules"
        SYSTEM_SETTINGS_MODULE_SOURCE_DIR="${SYSTEM_SETTINGS_RUNTIME_MODULE_SOURCE_DIR}")
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
    install(FILES
        assets/ui/hero-asset.png
        assets/ui/overview-asset.png
        assets/ui/date-asset.png
        assets/ui/region-asset.png
        assets/ui/network-asset.png
        assets/ui/theme-light.png
        assets/ui/theme-dark.png
        assets/ui/theme-follow.png
        assets/ui/theme-mercedes.png
        DESTINATION "${CMAKE_INSTALL_DATADIR}/infiltrator/system-settings/ui")
    install(FILES data/temporal-v3.provider
        DESTINATION "${CMAKE_INSTALL_DATADIR}/infiltrator/policy-providers"
        RENAME temporal-v3)
    install(FILES modules/date-time.settings-module
        DESTINATION "${CMAKE_INSTALL_DATADIR}/infiltrator/system-settings/modules")

    add_executable(system-settings-source-asset-builder
        packaging/source-asset-builder.c)
    ss_configure(system-settings-source-asset-builder)

    if(BUILD_TESTING)
        add_executable(system-settings-native-installer-test
            packaging/native-installer.c)
        ss_configure(system-settings-native-installer-test)
        add_test(NAME native-installer-help
            COMMAND system-settings-native-installer-test --help)
        add_test(NAME source-asset-builder-help
            COMMAND system-settings-source-asset-builder --help)
    endif()
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
        target_compile_definitions(linux-shell-test PRIVATE
            SYSTEM_SETTINGS_UI_ASSET_DIR="${CMAKE_INSTALL_FULL_DATADIR}/infiltrator/system-settings/ui"
            SYSTEM_SETTINGS_UI_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/ui"
            SYSTEM_SETTINGS_MODULE_DIR="${CMAKE_INSTALL_FULL_DATADIR}/infiltrator/system-settings/modules"
            SYSTEM_SETTINGS_MODULE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/modules")
        target_link_libraries(linux-shell-test PRIVATE
            system-settings-date-time-linux system-settings-linux-ui
            system-settings-core system-settings-platform PkgConfig::GTK4)
        find_program(SS_XVFB_RUN xvfb-run)
        if(SS_XVFB_RUN)
            add_test(NAME linux-shell COMMAND ${SS_XVFB_RUN} -a $<TARGET_FILE:linux-shell-test>)
            set_tests_properties(linux-shell PROPERTIES TIMEOUT 30 ENVIRONMENT
                "G_DEBUG=fatal-criticals;GTK_A11Y=none;GSK_RENDERER=cairo;GSETTINGS_BACKEND=memory")
            add_test(
                NAME linux-shell-scale2
                COMMAND ${SS_XVFB_RUN} -a -s "-screen 0 1920x1080x24"
                    $<TARGET_FILE:linux-shell-test>)
            set_tests_properties(linux-shell-scale2 PROPERTIES TIMEOUT 30 ENVIRONMENT
                "G_DEBUG=fatal-criticals;GTK_A11Y=none;GSK_RENDERER=cairo;GSETTINGS_BACKEND=memory;GDK_SCALE=2")
            add_test(
                NAME linux-shell-accessibility
                COMMAND ${SS_XVFB_RUN} -a $<TARGET_FILE:linux-shell-test>)
            set_tests_properties(linux-shell-accessibility PROPERTIES TIMEOUT 30 ENVIRONMENT
                "G_DEBUG=fatal-criticals;GSK_RENDERER=cairo;GSETTINGS_BACKEND=memory")
        else()
            message(WARNING "xvfb-run missing: linux-shell regression is not registered")
        endif()
        add_executable(policy-file-observer-test
            tests/policy-file-observer-test.c
            src/modules/date-time/policy-file-observer.c)
        ss_configure(policy-file-observer-test)
        target_include_directories(policy-file-observer-test PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/modules/date-time")
        target_link_libraries(policy-file-observer-test PRIVATE
            PkgConfig::GIO2)
        add_test(NAME policy-file-observer
            COMMAND policy-file-observer-test)

        add_executable(home-temporal-presentation-test
            tests/home-temporal-presentation-test.c)
        ss_configure(home-temporal-presentation-test)
        target_include_directories(home-temporal-presentation-test PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/modules/date-time")
        target_link_libraries(home-temporal-presentation-test PRIVATE
            system-settings-date-time-linux system-settings-core
            system-settings-platform InfiltratrCommon::Portable PkgConfig::GTK4)
        add_test(NAME home-temporal-presentation
            COMMAND home-temporal-presentation-test)

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

        add_executable(policy-concurrency-test
            tests/policy-concurrency-test.c)
        ss_configure(policy-concurrency-test)
        target_link_libraries(policy-concurrency-test PRIVATE
            system-settings-core
            system-settings-platform
            InfiltratrCommon::Common
            PkgConfig::GIO2)
        add_test(
            NAME policy-concurrency
            COMMAND policy-concurrency-test $<TARGET_FILE:system-settings-time>)

        add_executable(location-concurrency-test
            tests/location-concurrency-test.c)
        ss_configure(location-concurrency-test)
        target_link_libraries(location-concurrency-test PRIVATE
            system-settings-core
            system-settings-platform
            InfiltratrCommon::Common
            PkgConfig::GIO2)
        add_test(NAME location-concurrency COMMAND location-concurrency-test)

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
    # Keep package staging and the paths compiled into the executable in lockstep.
    set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()

include(CPack)
