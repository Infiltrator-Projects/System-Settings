#!/usr/bin/env python3
from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    if text.count(old) != 1:
        raise SystemExit(f"expected exactly one migration anchor in {path}: {old[:100]!r}")
    p.write_text(text.replace(old, new, 1))


Path("cmake/InfiltratorNativeFilesystem.cmake").write_text(r'''# SPDX-License-Identifier: GPL-3.0-or-later

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
''')

Path("data/infiltrator-native-path.sh").write_text(r'''# InfiltratorOS native PATH.
case ":$PATH:" in
  *:/Programs:*) ;;
  *) PATH="/Programs:$PATH" ;;
esac
case ":$PATH:" in
  *:/System/Commands:*) ;;
  *) PATH="/System/Commands:$PATH" ;;
esac
export PATH
''')

replace_once(
    "CMakeLists.txt",
    "project(InfiltratrSystemSettings VERSION \"${SYSTEM_SETTINGS_VERSION}\" LANGUAGES C)\ninclude(CTest)\n",
    "project(InfiltratrSystemSettings VERSION \"${SYSTEM_SETTINGS_VERSION}\" LANGUAGES C)\ninclude(CTest)\ninclude(\"${CMAKE_CURRENT_SOURCE_DIR}/cmake/InfiltratorNativeFilesystem.cmake\")\n",
)

replace_once(
    "CMakeLists.txt",
    '''    install(TARGETS system-settings\n        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")\n''',
    '''    infiltrator_install_native_filesystem_skeleton()\n    install(TARGETS system-settings\n        RUNTIME DESTINATION "/Programs")\n    install(FILES data/infiltrator-native-path.sh\n        DESTINATION "/etc/profile.d"\n        RENAME "91-infiltrator-system-settings-path.sh")\n''',
)

replace_once(
    "CMakeLists.txt",
    '''install(TARGETS system-settings-time\n    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")\n''',
    '''if(CMAKE_SYSTEM_NAME STREQUAL "Linux")\n    install(TARGETS system-settings-time\n        RUNTIME DESTINATION "/System/Commands")\nelse()\n    install(TARGETS system-settings-time\n        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")\nendif()\n''',
)

replace_once(
    "CMakeLists.txt",
    '''    set(CPACK_GENERATOR "DEB")\n    # Keep package staging and the paths compiled into the executable in lockstep.\n    set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")\n''',
    '''    set(CPACK_GENERATOR "DEB")\n    # Absolute native paths (/Programs and /System/Commands) must be staged\n    # under CPack's package root, while conventional passive resources remain\n    # below the /usr prefix.\n    set(CPACK_SET_DESTDIR ON)\n    set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")\n''',
)

replace_once(
    "data/org.infiltrator.SystemSettings.desktop",
    "Exec=system-settings\n",
    "Exec=/Programs/system-settings\n",
)

installer = Path("packaging/native-installer.c")
text = installer.read_text()
install_anchor = '''        if (run_command(NULL, install_argv) != 0) {\n            (void)remove_tree(work);\n            fail("APT installation failed.");\n        }\n'''
install_replacement = install_anchor + '''        if (access("/Programs/system-settings", X_OK) != 0 ||\n            access("/System/Commands/system-settings-time", X_OK) != 0) {\n            (void)remove_tree(work);\n            fail("native package did not install System Settings into the InfiltratorOS executable namespaces.");\n        }\n'''
if text.count(install_anchor) != 1:
    raise SystemExit("native installer APT installation anchor missing or ambiguous")
installer.write_text(text.replace(install_anchor, install_replacement, 1))

ci = Path(".github/workflows/ci.yml")
text = ci.read_text()
old = '''          ui="$root/usr/share/infiltrator/system-settings/ui"\n          binary="$root/usr/bin/system-settings"\n          test -x "$binary"\n          grep -aFq '/usr/share/infiltrator/system-settings/ui' "$binary"\n'''
new = '''          ui="$root/usr/share/infiltrator/system-settings/ui"\n          binary="$root/Programs/system-settings"\n          command="$root/System/Commands/system-settings-time"\n          test -x "$binary"\n          test -x "$command"\n          test -f "$root/etc/profile.d/91-infiltrator-system-settings-path.sh"\n          for dir in \\
            Programs System/Commands System/Libraries System/Devices System/Filesystems \\
            System/Kernel/Images System/Kernel/Modules System/Kernel/Firmware \\
            System/Resources/Calendars System/Resources/Clocks System/Resources/Fonts \\
            System/Resources/Locales System/Resources/Icons System/Resources/Themes \\
            System/Preferences Users Volumes Temporary; do\n            test -d "$root/$dir"\n          done\n          test ! -e "$root/usr/bin/system-settings"\n          test ! -e "$root/usr/bin/system-settings-time"\n          test ! -e "$root/System/Programs"\n          grep -Fqx 'Exec=/Programs/system-settings' "$desktop"\n          grep -aFq '/usr/share/infiltrator/system-settings/ui' "$binary"\n'''
if text.count(old) != 1:
    raise SystemExit("System Settings CI package path anchor missing or ambiguous")
ci.write_text(text.replace(old, new, 1))

Path("VERSION").write_text("0.4.110\n")

changelog = Path("CHANGELOG.md")
text = changelog.read_text()
heading = "# Changelog\n"
entry = '''# Changelog\n\n## 0.4.110 - 2026-10-17\n\n- Install the Linux control-panel application at `/Programs/system-settings` and its genuine command helper at `/System/Commands/system-settings-time`.\n- Establish the native InfiltratorOS filesystem skeleton and `/System/Commands:/Programs` login-shell PATH when System Settings is installed standalone.\n- Use the absolute native application path from the desktop launcher and install no System Settings executable or compatibility link under `/usr/bin`.\n- Extend package qualification so `/usr/bin` executable entries and the obsolete `/System/Programs` split cannot return.\n'''
if heading not in text:
    raise SystemExit("System Settings changelog heading missing")
changelog.write_text(text.replace(heading, entry, 1))

architecture = Path("docs/ARCHITECTURE.md")
text = architecture.read_text()
section = '''\n\n## Native Linux filesystem ownership\n\nSystem Settings is user-launchable, so its Linux GUI belongs to the single application namespace at `/Programs/system-settings`, even when bundled with InfiltratorOS. The non-GUI `system-settings-time` administrative interface is a genuine command and belongs at `/System/Commands/system-settings-time`. `/System` otherwise remains operating-system infrastructure; there is deliberately no `/System/Programs`. Standalone System Settings installation creates the common native skeleton when necessary and never adds product executables or compatibility links to `/usr/bin`.\n'''
if "## Native Linux filesystem ownership" not in text:
    architecture.write_text((text.rstrip() + section).rstrip() + "\n")

readme = Path("README.md")
text = readme.read_text()
section = '''\n\n## Native Linux installation paths\n\nThe Linux GUI installs at `/Programs/system-settings`. Its `system-settings-time` command helper installs at `/System/Commands/system-settings-time`. Debian and hardware-native installation create the InfiltratorOS native directory skeleton when needed and add `/System/Commands:/Programs` to future login-shell PATHs. No System Settings executable or compatibility link is installed under `/usr/bin`; the desktop launcher uses `/Programs/system-settings` directly.\n'''
if "## Native Linux installation paths" not in text:
    readme.write_text((text.rstrip() + section).rstrip() + "\n")

# Final source-level invariants for the migration itself.
cmake_text = Path("CMakeLists.txt").read_text()
if 'RUNTIME DESTINATION "/Programs"' not in cmake_text:
    raise SystemExit("System Settings application native destination missing")
if 'RUNTIME DESTINATION "/System/Commands"' not in cmake_text:
    raise SystemExit("System Settings command native destination missing")
if "/System/Programs" in cmake_text:
    raise SystemExit("obsolete /System/Programs returned to CMake")
