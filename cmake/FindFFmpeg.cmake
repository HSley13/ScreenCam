# FindFFmpeg
# ----------
# Finds the FFmpeg development files (headers + libraries) used by the video exporter:
# libavformat, libavcodec, libavutil and libswscale.
#
# Why not just pkg-config: Windows rarely has it, and on macOS the FFmpeg that Qt's
# multimedia backend was built against (scripts/build-qt-ffmpeg.sh -> ~/.local/ffmpeg-<ver>)
# is the one we *want*, so the app and Qt share a single copy of the libraries.
#
# Search order:
#   1. -DFFMPEG_DIR=<prefix> or the FFMPEG_DIR environment variable
#   2. ~/.local/ffmpeg-*   (newest first; what scripts/build-qt-ffmpeg.sh installs)
#   3. pkg-config, then the usual system locations (Homebrew, /usr, vcpkg's prefix ...)
#
# Result:
#   FFmpeg_FOUND, FFmpeg_VERSION (libavcodec's), FFmpeg_INCLUDE_DIR
#   Imported targets FFmpeg::avformat, FFmpeg::avcodec, FFmpeg::avutil, FFmpeg::swscale

set(_ffmpeg_hints)
if(FFMPEG_DIR)
    list(APPEND _ffmpeg_hints "${FFMPEG_DIR}")
endif()
if(DEFINED ENV{FFMPEG_DIR})
    list(APPEND _ffmpeg_hints "$ENV{FFMPEG_DIR}")
endif()
if(DEFINED ENV{HOME})
    file(GLOB _ffmpeg_local LIST_DIRECTORIES true "$ENV{HOME}/.local/ffmpeg-*")
    list(SORT _ffmpeg_local COMPARE NATURAL ORDER DESCENDING)
    list(APPEND _ffmpeg_hints ${_ffmpeg_local})
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(_ffmpeg_pc QUIET libavformat libavcodec libavutil libswscale)
endif()

find_path(FFmpeg_INCLUDE_DIR
    NAMES libavcodec/avcodec.h
    HINTS ${_ffmpeg_hints} ${_ffmpeg_pc_INCLUDE_DIRS}
    PATH_SUFFIXES include)

set(_ffmpeg_components avformat avcodec avutil swscale)
set(_ffmpeg_required_vars FFmpeg_INCLUDE_DIR)
foreach(_component IN LISTS _ffmpeg_components)
    find_library(FFmpeg_${_component}_LIBRARY
        NAMES ${_component}
        HINTS ${_ffmpeg_hints} ${_ffmpeg_pc_LIBRARY_DIRS}
        PATH_SUFFIXES lib lib64)
    list(APPEND _ffmpeg_required_vars FFmpeg_${_component}_LIBRARY)
endforeach()

# Version from libavcodec/version_major.h + version.h (e.g. 61.19.101 for FFmpeg 7.1).
if(FFmpeg_INCLUDE_DIR AND EXISTS "${FFmpeg_INCLUDE_DIR}/libavcodec/version.h")
    file(READ "${FFmpeg_INCLUDE_DIR}/libavcodec/version.h" _ffmpeg_version_h)
    set(_ffmpeg_major_h "${FFmpeg_INCLUDE_DIR}/libavcodec/version_major.h")
    if(EXISTS "${_ffmpeg_major_h}")
        file(READ "${_ffmpeg_major_h}" _ffmpeg_major_text)
        string(APPEND _ffmpeg_version_h "${_ffmpeg_major_text}")
    endif()
    string(REGEX MATCH "#define LIBAVCODEC_VERSION_MAJOR +([0-9]+)" _ "${_ffmpeg_version_h}")
    set(_major "${CMAKE_MATCH_1}")
    string(REGEX MATCH "#define LIBAVCODEC_VERSION_MINOR +([0-9]+)" _ "${_ffmpeg_version_h}")
    set(_minor "${CMAKE_MATCH_1}")
    string(REGEX MATCH "#define LIBAVCODEC_VERSION_MICRO +([0-9]+)" _ "${_ffmpeg_version_h}")
    set(FFmpeg_VERSION "${_major}.${_minor}.${CMAKE_MATCH_1}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFmpeg
    REQUIRED_VARS ${_ffmpeg_required_vars}
    VERSION_VAR FFmpeg_VERSION)

if(FFmpeg_FOUND)
    foreach(_component IN LISTS _ffmpeg_components)
        if(NOT TARGET FFmpeg::${_component})
            add_library(FFmpeg::${_component} UNKNOWN IMPORTED)
            set_target_properties(FFmpeg::${_component} PROPERTIES
                IMPORTED_LOCATION "${FFmpeg_${_component}_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${FFmpeg_INCLUDE_DIR}")
        endif()
    endforeach()
endif()

mark_as_advanced(FFmpeg_INCLUDE_DIR)
