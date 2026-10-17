# SPDX-License-Identifier: GPL-3.0-or-later

# Every standalone Infiltrator package establishes the same native directory
# contract. Product binaries own their files; these shared directories are
# structural and may be present before InfiltratorOS itself is installed.
function(infiltrator_install_native_filesystem_skeleton)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        return()
    endif()

    install(CODE [=[
        set(_infiltrator_root "$ENV{DESTDIR}")
        foreach(_infiltrator_directory IN ITEMS
            "/System/Commands"
            "/System/Libraries"
            "/System/Devices"
            "/System/Filesystems"
            "/System/Kernel/Images"
            "/System/Kernel/Modules"
            "/System/Kernel/Firmware"
            "/System/Resources/Calendars"
            "/System/Resources/Clocks"
            "/System/Resources/Fonts"
            "/System/Resources/Locales"
            "/System/Resources/Icons"
            "/System/Resources/Themes"
            "/System/Preferences"
            "/Programs"
            "/Users"
            "/Volumes"
            "/Temporary")
            file(MAKE_DIRECTORY "${_infiltrator_root}${_infiltrator_directory}")
        endforeach()
    ]=])
endfunction()
