# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" SYSTEM_SETTINGS_VERSION)
string(STRIP "${SYSTEM_SETTINGS_VERSION}" SYSTEM_SETTINGS_VERSION)
if(NOT SYSTEM_SETTINGS_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain major.minor.patch")
endif()

project(InfiltratrSystemSettings VERSION "${SYSTEM_SETTINGS_VERSION}" LANGUAGES C)
include(CTest)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/InfiltratorNativeFilesystem.cmake")

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

if(NOT EXISTS "${SYSTEM_SETTINGS_COMMON_DIR}/CMakeLists.txt" OR
   NOT EXISTS "${SYSTEM_SETTINGS_COMMON_DIR}/VERSION")
    message(FATAL_ERROR "Pinned Libraries submodule is required.")
endif()
file(READ "${SYSTEM_SETTINGS_COMMON_DIR}/VERSION"
    SYSTEM_SETTINGS_COMMON_VERSION)
string(STRIP "${SYSTEM_SETTINGS_COMMON_VERSION}"
    SYSTEM_SETTINGS_COMMON_VERSION)
if(NOT SYSTEM_SETTINGS_COMMON_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR
        "Pinned Common VERSION is malformed: ${SYSTEM_SETTINGS_COMMON_VERSION}")
endif()

# The repository gitlink is the single Common revision authority. Do not mirror
# that SHA (or the Common version) here: duplicated pins drift during upgrades.
# Official source archives carry the gitlink-derived commit marker plus a full
# byte manifest because the parent Git metadata is intentionally absent there.
find_package(Git QUIET)
set(SYSTEM_SETTINGS_COMMON_PROVENANCE
    "${SYSTEM_SETTINGS_COMMON_DIR}/.system-settings-common-commit")
if(GIT_FOUND AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git" AND
   EXISTS "${SYSTEM_SETTINGS_COMMON_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_CURRENT_SOURCE_DIR}"
                rev-parse ":src/vendor/infiltratr-common"
        RESULT_VARIABLE SYSTEM_SETTINGS_COMMON_GITLINK_RESULT
        OUTPUT_VARIABLE SYSTEM_SETTINGS_COMMON_EXPECTED_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${SYSTEM_SETTINGS_COMMON_DIR}"
                rev-parse HEAD
        RESULT_VARIABLE SYSTEM_SETTINGS_COMMON_GIT_RESULT
        OUTPUT_VARIABLE SYSTEM_SETTINGS_COMMON_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(NOT SYSTEM_SETTINGS_COMMON_GITLINK_RESULT EQUAL 0 OR
       NOT SYSTEM_SETTINGS_COMMON_GIT_RESULT EQUAL 0 OR
       NOT SYSTEM_SETTINGS_COMMON_COMMIT STREQUAL
           SYSTEM_SETTINGS_COMMON_EXPECTED_COMMIT)
        message(FATAL_ERROR
            "Checked-out Common does not match the repository gitlink: "
            "expected ${SYSTEM_SETTINGS_COMMON_EXPECTED_COMMIT}; found "
            "${SYSTEM_SETTINGS_COMMON_COMMIT}")
    endif()
elseif(EXISTS "${SYSTEM_SETTINGS_COMMON_PROVENANCE}")
    file(READ "${SYSTEM_SETTINGS_COMMON_PROVENANCE}"
        SYSTEM_SETTINGS_COMMON_COMMIT)
    string(STRIP "${SYSTEM_SETTINGS_COMMON_COMMIT}"
        SYSTEM_SETTINGS_COMMON_COMMIT)
    string(LENGTH "${SYSTEM_SETTINGS_COMMON_COMMIT}"
        SYSTEM_SETTINGS_COMMON_COMMIT_LENGTH)
    if(NOT SYSTEM_SETTINGS_COMMON_COMMIT_LENGTH EQUAL 40 OR
       NOT SYSTEM_SETTINGS_COMMON_COMMIT MATCHES "^[0-9a-fA-F]+$")
        message(FATAL_ERROR
            "Exported Common provenance marker is malformed.")
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

# Generic product identity is deliberately independent of settings-domain code.
# The shell may consume this target without acquiring Date & Time ownership.
add_library(system-settings-app STATIC
    src/app/project-info.c)
ss_configure(system-settings-app)
target_compile_definitions(system-settings-app PUBLIC
    SYSTEM_SETTINGS_VERSION="${PROJECT_VERSION}"
    SYSTEM_SETTINGS_BUILD_PROFILE="${SYSTEM_SETTINGS_BUILD_PROFILE}")
target_link_libraries(system-settings-app PUBLIC InfiltratrCommon::Portable)

# Portable Date & Time domain logic. Generic shell targets must not link this.
add_library(system-settings-core STATIC
    src/modules/date-time/date-time-model.c
    src/modules/date-time/manual-time.c
    src/modules/date-time/native-clock-policy.c)
ss_configure(system-settings-core)
target_link_libraries(system-settings-core PUBLIC InfiltratrCommon::Portable)

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
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SystemSettingsTypography.cmake")

    add_library(system-settings-platform STATIC
        src/platform/linux/cinnamon-interface.c
        src/platform/linux/location-metadata.c
        src/platform/linux/regional-context.c
        src/platform/linux/system-time-service.c
        src/platform/linux/temporal-policy-store.c)
    target_link_libraries(system-settings-platform
        PUBLIC InfiltratrCommon::Common PkgConfig::GIO2)

    # Module-safe GTK helpers only. Shell-owned presentation infrastructure,
    # including the display-global theme provider, belongs to the shell target
    # so modules cannot acquire shell implementation through this shared edge.
    add_library(system-settings-linux-ui STATIC
        src/ui/linux/linux-ui-helpers.c)
    ss_configure(system-settings-linux-ui)
    target_compile_definitions(system-settings-linux-ui PRIVATE
        SYSTEM_SETTINGS_GENERIC_SHELL=1)
    target_include_directories(system-settings-linux-ui PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/linux")
    target_link_libraries(system-settings-linux-ui PUBLIC
        PkgConfig::GTK4
        InfiltratrCommon::Portable)

    add_library(system-settings-date-time-linux STATIC
        src/modules/date-time/home-temporal-presentation.c
        src/modules/date-time/policy-file-observer.c
        src/modules/date-time/linux-date-time-panel.c
        src/modules/date-time/linux-date-time-system-time.c
        src/modules/date-time/linux-date-time-system-time-boundary.c
        src/modules/date-time/linux-date-time-preview.c
        src/modules/date-time/linux-date-time-preview-boundary.c
        src/modules/date-time/linux-date-time-location-boundary.c
        src/modules/date-time/linux-date-time-location.c
        src/modules/date-time/linux-date-time-panel-ui.c
        src/platform/linux/location-search.c)
    # Locality is protected by the C type system rather than by grepping its
    # source. The complete panel-private header rejects inclusion whenever this
    # source-specific marker is present.
    set_source_files_properties(
        src/modules/date-time/linux-date-time-location.c
        PROPERTIES COMPILE_DEFINITIONS
            SYSTEM_SETTINGS_DATE_TIME_LOCATION_BOUNDARY=1)
    set_source_files_properties(
        src/modules/date-time/linux-date-time-preview.c
        PROPERTIES COMPILE_DEFINITIONS
            SYSTEM_SETTINGS_DATE_TIME_PREVIEW_BOUNDARY=1)
    set_source_files_properties(
        src/modules/date-time/linux-date-time-system-time.c
        PROPERTIES COMPILE_DEFINITIONS
            SYSTEM_SETTINGS_DATE_TIME_SYSTEM_TIME_BOUNDARY=1)
    ss_configure(system-settings-date-time-linux)
    target_include_directories(system-settings-date-time-linux PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/modules/date-time")
    target_link_libraries(system-settings-date-time-linux PRIVATE
        system-settings-core
        system-settings-platform
        system-settings-linux-ui
        InfiltratrCommon::Common
        InfiltratrCommon::Portable
        PkgConfig::GTK4
        PkgConfig::GEOCODE)

    # Composition root for today's built-in modules. Shell code consumes only
    # builtin-module-host.h; private module headers never enter the shell target.
    add_library(system-settings-linux-modules STATIC
        src/app/linux-builtin-module-host.c
        src/app/home-temporal-provider.c)
    ss_configure(system-settings-linux-modules)
    target_include_directories(system-settings-linux-modules
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/app"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/modules/date-time")
    target_link_libraries(system-settings-linux-modules PRIVATE
        system-settings-date-time-linux
        PkgConfig::GIO2
        PkgConfig::GTK4)

    set(SYSTEM_SETTINGS_RUNTIME_UI_SOURCE_DIR "")
    set(SYSTEM_SETTINGS_RUNTIME_MODULE_SOURCE_DIR "")
    if(SYSTEM_SETTINGS_BUILD_PROFILE STREQUAL "cmake" OR
       SYSTEM_SETTINGS_BUILD_PROFILE STREQUAL "development")
        set(SYSTEM_SETTINGS_RUNTIME_UI_SOURCE_DIR
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui")
        set(SYSTEM_SETTINGS_RUNTIME_MODULE_SOURCE_DIR
            "${CMAKE_CURRENT_SOURCE_DIR}/modules")
    endif()

    add_library(system-settings-linux-shell STATIC
        src/shell/linux/shell-command.c
        src/shell/linux/home-status.c
        src/shell/linux/home-page.c
        src/shell/linux/linux-theme.c
        src/shell/linux/shell-module-catalog.c
        src/shell/linux/shell-window.c)
    ss_configure(system-settings-linux-shell)
    target_include_directories(system-settings-linux-shell PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shell/linux")
    target_compile_definitions(system-settings-linux-shell PRIVATE
        SYSTEM_SETTINGS_GENERIC_SHELL=1
        SYSTEM_SETTINGS_UI_ASSET_DIR="${CMAKE_INSTALL_FULL_DATADIR}/infiltrator/system-settings/ui"
        SYSTEM_SETTINGS_UI_SOURCE_DIR="${SYSTEM_SETTINGS_RUNTIME_UI_SOURCE_DIR}"
        SYSTEM_SETTINGS_MODULE_DIR="${CMAKE_INSTALL_FULL_DATADIR}/infiltrator/system-settings/modules"
        SYSTEM_SETTINGS_MODULE_SOURCE_DIR="${SYSTEM_SETTINGS_RUNTIME_MODULE_SOURCE_DIR}")
    target_link_libraries(system-settings-linux-shell PRIVATE
        system-settings-linux-modules
        system-settings-linux-ui
        system-settings-app
        InfiltratrCommon::Portable
        PkgConfig::GTK4)

    add_executable(system-settings
        src/shell/linux/main.c)
    ss_configure(system-settings)
    target_compile_definitions(system-settings PRIVATE
        SYSTEM_SETTINGS_GENERIC_SHELL=1)
    target_include_directories(system-settings PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shell/linux")
    target_link_libraries(system-settings PRIVATE
        system-settings-linux-shell
        system-settings-app
        InfiltratrCommon::Portable
        PkgConfig::GTK4)

    # Keep shell/module decoupling non-regressible at configure time.
    get_target_property(
        SS_SHELL_LIBRARY_DIRECT_LINKS system-settings-linux-shell LINK_LIBRARIES)
    foreach(SS_FORBIDDEN_SHELL_LINK
            system-settings-core
            system-settings-platform
            system-settings-date-time-linux)
        list(FIND SS_SHELL_LIBRARY_DIRECT_LINKS "${SS_FORBIDDEN_SHELL_LINK}"
            SS_FORBIDDEN_SHELL_LINK_INDEX)
        if(NOT SS_FORBIDDEN_SHELL_LINK_INDEX EQUAL -1)
            message(FATAL_ERROR
                "Generic Linux shell must reach modules through "
                "system-settings-linux-modules, not ${SS_FORBIDDEN_SHELL_LINK}.")
        endif()
    endforeach()

    get_target_property(SS_EXECUTABLE_DIRECT_LINKS system-settings LINK_LIBRARIES)
    foreach(SS_FORBIDDEN_EXECUTABLE_LINK
            system-settings-core
            system-settings-platform
            system-settings-date-time-linux)
        list(FIND SS_EXECUTABLE_DIRECT_LINKS "${SS_FORBIDDEN_EXECUTABLE_LINK}"
            SS_FORBIDDEN_EXECUTABLE_LINK_INDEX)
        if(NOT SS_FORBIDDEN_EXECUTABLE_LINK_INDEX EQUAL -1)
            message(FATAL_ERROR
                "System Settings executable must link the generic shell, not "
                "${SS_FORBIDDEN_EXECUTABLE_LINK} directly.")
        endif()
    endforeach()

    get_target_property(
        SS_DATE_TIME_DIRECT_INCLUDES
        system-settings-date-time-linux
        INCLUDE_DIRECTORIES)
    set(SS_SHELL_SOURCE_INCLUDE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shell/linux")
    list(FIND SS_DATE_TIME_DIRECT_INCLUDES "${SS_SHELL_SOURCE_INCLUDE}"
        SS_DATE_TIME_SHELL_INCLUDE_INDEX)
    if(NOT SS_DATE_TIME_SHELL_INCLUDE_INDEX EQUAL -1)
        message(FATAL_ERROR
            "Date & Time must consume Linux UI through system-settings-linux-ui; "
            "it must not add the shell source directory directly.")
    endif()
    get_target_property(
        SS_LINUX_UI_PUBLIC_INCLUDES
        system-settings-linux-ui
        INTERFACE_INCLUDE_DIRECTORIES)
    list(FIND SS_LINUX_UI_PUBLIC_INCLUDES "${SS_SHELL_SOURCE_INCLUDE}"
        SS_LINUX_UI_SHELL_INCLUDE_INDEX)
    if(NOT SS_LINUX_UI_SHELL_INCLUDE_INDEX EQUAL -1)
        message(FATAL_ERROR
            "Shared Linux UI infrastructure must not expose the shell source directory.")
    endif()
    get_target_property(
        SS_LINUX_UI_SOURCES
        system-settings-linux-ui
        SOURCES)
    foreach(SS_LINUX_UI_SOURCE IN LISTS SS_LINUX_UI_SOURCES)
        if(SS_LINUX_UI_SOURCE MATCHES "(^|/)src/shell/")
            message(FATAL_ERROR
                "Shared Linux UI infrastructure must not compile shell-owned source: ${SS_LINUX_UI_SOURCE}")
        endif()
    endforeach()
    get_target_property(
        SS_MODULE_ADAPTER_PUBLIC_INCLUDES
        system-settings-linux-modules
        INTERFACE_INCLUDE_DIRECTORIES)
    list(FIND SS_MODULE_ADAPTER_PUBLIC_INCLUDES "${SS_SHELL_SOURCE_INCLUDE}"
        SS_MODULE_ADAPTER_SHELL_INCLUDE_INDEX)
    if(NOT SS_MODULE_ADAPTER_SHELL_INCLUDE_INDEX EQUAL -1)
        message(FATAL_ERROR
            "The module composition adapter must not expose shell-owned headers.")
    endif()
    unset(SS_MODULE_ADAPTER_SHELL_INCLUDE_INDEX)
    unset(SS_MODULE_ADAPTER_PUBLIC_INCLUDES)
    unset(SS_LINUX_UI_SOURCE)
    unset(SS_LINUX_UI_SOURCES)
    unset(SS_LINUX_UI_SHELL_INCLUDE_INDEX)
    unset(SS_LINUX_UI_PUBLIC_INCLUDES)
    unset(SS_DATE_TIME_SHELL_INCLUDE_INDEX)
    unset(SS_SHELL_SOURCE_INCLUDE)
    unset(SS_DATE_TIME_DIRECT_INCLUDES)
    unset(SS_FORBIDDEN_EXECUTABLE_LINK_INDEX)
    unset(SS_EXECUTABLE_DIRECT_LINKS)
    unset(SS_FORBIDDEN_SHELL_LINK_INDEX)
    unset(SS_SHELL_LIBRARY_DIRECT_LINKS)

    infiltrator_install_native_filesystem_skeleton()
    install(TARGETS system-settings
        RUNTIME DESTINATION "/Programs")
    install(FILES data/infiltrator-native-path.sh
        DESTINATION "/etc/profile.d"
        RENAME "91-infiltrator-system-settings-path.sh")
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
    target_link_libraries(project-info-test PRIVATE system-settings-app)
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
            "${CMAKE_CURRENT_SOURCE_DIR}/src/shell/linux")
        target_link_libraries(linux-shell-test PRIVATE
            system-settings-linux-shell
            InfiltratrCommon::Common
            PkgConfig::GTK4)
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

        add_executable(home-temporal-provider-test
            tests/home-temporal-provider-test.c)
        ss_configure(home-temporal-provider-test)
        target_link_libraries(home-temporal-provider-test PRIVATE
            system-settings-linux-modules PkgConfig::GTK4)
        add_test(NAME home-temporal-provider
            COMMAND home-temporal-provider-test)

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

        add_library(calendar-date-runtime-fixture SHARED
            tests/calendar-date-runtime-fixture.c)
        ss_configure(calendar-date-runtime-fixture)
        target_link_libraries(calendar-date-runtime-fixture PRIVATE
            InfiltratrCommon::Portable)
        set_target_properties(calendar-date-runtime-fixture PROPERTIES
            PREFIX ""
            OUTPUT_NAME "calendar-date-runtime-fixture")

        add_executable(calendar-date-provider-test
            tests/calendar-date-provider-test.c)
        ss_configure(calendar-date-provider-test)
        target_link_libraries(calendar-date-provider-test PRIVATE
            InfiltratrCommon::Common)
        add_test(NAME calendar-date-provider
            COMMAND calendar-date-provider-test
                $<TARGET_FILE:calendar-date-runtime-fixture>)
    endif()
endif()


if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(TARGETS system-settings-time
        RUNTIME DESTINATION "/System/Commands")
else()
    install(TARGETS system-settings-time
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()


set(CPACK_PACKAGE_NAME "system-settings")
set(CPACK_PACKAGE_VENDOR "Infiltrator Projects")
set(CPACK_PACKAGE_CONTACT "Shannon Smith <infiltratr@yandex.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Native system-wide settings shell for Infiltrator applications")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")

if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB")
    # Absolute native paths (/Programs and /System/Commands) must be staged
    # under CPack's package root, while conventional passive resources remain
    # below the /usr prefix.
    set(CPACK_SET_DESTDIR ON)
    set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/ci/temporal-providers.json" SETTINGS_TEMPORAL_PROVIDERS)
    string(JSON SETTINGS_TEMPORAL_PROVIDER_COUNT LENGTH "${SETTINGS_TEMPORAL_PROVIDERS}")
    math(EXPR SETTINGS_TEMPORAL_PROVIDER_LAST "${SETTINGS_TEMPORAL_PROVIDER_COUNT} - 1")
    set(SETTINGS_TEMPORAL_SUGGESTS)
    foreach(index RANGE 0 ${SETTINGS_TEMPORAL_PROVIDER_LAST})
        string(JSON provider_package GET "${SETTINGS_TEMPORAL_PROVIDERS}" ${index} package)
        list(APPEND SETTINGS_TEMPORAL_SUGGESTS "${provider_package}")
    endforeach()
    list(JOIN SETTINGS_TEMPORAL_SUGGESTS ", " CPACK_DEBIAN_PACKAGE_SUGGESTS)
endif()

include(CPack)
