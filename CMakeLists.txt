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
    endif()
endfunction()

add_library(system-settings-core STATIC
    src/app/project-info.c
    src/modules/date-time/date-time-model.c)
ss_configure(system-settings-core)
target_compile_definitions(system-settings-core PUBLIC
    SYSTEM_SETTINGS_VERSION="${PROJECT_VERSION}"
    SYSTEM_SETTINGS_BUILD_PROFILE="${SYSTEM_SETTINGS_BUILD_PROFILE}")
target_link_libraries(system-settings-core PUBLIC InfiltratrCommon::Portable)

if(WIN32)
    add_library(system-settings-platform STATIC
        src/platform/windows/temporal-policy-store.c)
    target_link_libraries(system-settings-platform
        PUBLIC InfiltratrCommon::Portable shell32 ole32)
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GIO2 REQUIRED IMPORTED_TARGET gio-2.0)
    pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4>=4.6)

    add_library(system-settings-platform STATIC
        src/platform/linux/cinnamon-interface.c
        src/platform/linux/temporal-policy-store.c)
    target_link_libraries(system-settings-platform
        PUBLIC InfiltratrCommon::Common PkgConfig::GIO2)

    add_executable(system-settings
        src/shell/linux/main.c)
    ss_configure(system-settings)
    target_link_libraries(system-settings PRIVATE
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

    add_executable(project-info-test tests/project-info-test.c)
    ss_configure(project-info-test)
    target_link_libraries(project-info-test PRIVATE system-settings-core)
    add_test(NAME project-info COMMAND project-info-test)

    if(UNIX)
        add_executable(temporal-policy-store-test
            tests/temporal-policy-store-test.c)
        ss_configure(temporal-policy-store-test)
        target_link_libraries(temporal-policy-store-test PRIVATE
            system-settings-platform)
        add_test(NAME temporal-policy-store COMMAND temporal-policy-store-test)
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
