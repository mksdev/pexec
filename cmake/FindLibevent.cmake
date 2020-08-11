# - Try to find the LibEvent config processing library
# Once done this will define
#
# Libevent_FOUND - System has LibEvent
# Libevent_INCLUDE_DIR - the LibEvent include directory
# Libevent_LIBRARIES 0 The libraries needed to use LibEvent
find_path(Libevent_INCLUDE_DIR NAMES event.h)
find_library(Libevent_LIBRARY NAMES event)
find_library(Libevent_CORE NAMES event_core)
find_library(Libevent_EXTRA NAMES event_extra)
if (NOT EVHTP_DISABLE_EVTHR)
    find_library(Libevent_THREAD NAMES event_pthreads)
endif ()
if (NOT EVHTP_DISABLE_SSL)
    find_library(Libevent_SSL NAMES event_openssl)
endif ()
include(FindPackageHandleStandardArgs)
set(Libevent_INCLUDE_DIRS ${Libevent_INCLUDE_DIR})
set(Libevent_LIBRARIES
        ${Libevent_LIBRARY}
        ${Libevent_SSL}
        ${Libevent_CORE}
        ${Libevent_EXTRA}
        ${Libevent_THREAD}
        ${Libevent_EXTRA})
find_package_handle_standard_args(Libevent DEFAULT_MSG Libevent_LIBRARIES Libevent_INCLUDE_DIR)
mark_as_advanced(Libevent_INCLUDE_DIRS Libevent_LIBRARIES)