# SPDX-License-Identifier: GPL-3.0-or-later
file(REMOVE_RECURSE "${ROOT}")
file(MAKE_DIRECTORY "${ROOT}")
set(ENV{XDG_CONFIG_HOME} "${ROOT}")
set(ENV{GSETTINGS_BACKEND} "memory")
execute_process(COMMAND "${CLI}" --clock standard-24 RESULT_VARIABLE status OUTPUT_QUIET)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Initial CLI write failed")
endif()
file(READ "${ROOT}/infiltrator/presentation.conf" before)
execute_process(COMMAND "${CLI}" --clock decimal --calendar invalid RESULT_VARIABLE status OUTPUT_QUIET ERROR_QUIET)
if(status EQUAL 0)
    message(FATAL_ERROR "Malformed command succeeded")
endif()
file(READ "${ROOT}/infiltrator/presentation.conf" after)
if(NOT before STREQUAL after)
    message(FATAL_ERROR "Failed command partially persisted earlier options")
endif()
execute_process(
    COMMAND "${CLI}" --clock decimal --seconds on
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Valid multi-option command failed")
endif()
if(NOT output MATCHES "source=infiltrator-policy")
    message(FATAL_ERROR "Successful policy save reported stale source provenance")
endif()
file(READ "${ROOT}/infiltrator/presentation.conf" after)
if(NOT after MATCHES "clock-mode=decimal" OR NOT after MATCHES "show-seconds=true")
    message(FATAL_ERROR "Multi-option transaction lost a setting")
endif()

execute_process(
    COMMAND "${CLI}" --location -36.3949 145.3610
    RESULT_VARIABLE status
    OUTPUT_QUIET)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "CLI location transaction failed")
endif()
set(location_file "${ROOT}/infiltrator/system-settings/location.ini")
if(NOT EXISTS "${location_file}")
    message(FATAL_ERROR "CLI location transaction did not publish locality metadata")
endif()
file(READ "${location_file}" location_metadata)
if(NOT location_metadata MATCHES "Name=Custom coordinates" OR
   NOT location_metadata MATCHES "Latitude=-36.39" OR
   NOT location_metadata MATCHES "Longitude=145.36")
    message(FATAL_ERROR "CLI locality metadata does not match committed coordinates")
endif()

execute_process(
    COMMAND "${CLI}" --clear-location
    RESULT_VARIABLE status
    OUTPUT_QUIET)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "CLI clear-location transaction failed")
endif()
if(EXISTS "${location_file}")
    message(FATAL_ERROR "CLI clear-location left stale locality metadata")
endif()
file(READ "${ROOT}/infiltrator/presentation.conf" after)
if(NOT after MATCHES "location-configured=false")
    message(FATAL_ERROR "CLI clear-location did not clear temporal location authority")
endif()

file(REMOVE_RECURSE "${ROOT}")
