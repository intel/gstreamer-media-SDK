cmake_minimum_required (VERSION 3.0)

# This project use C source code
project (GstMfx C)

include(CMakeDependentOption)

option (DEBUG "Turn on debug build." OFF)

option (MFX_DECODER "Build MSDK decoder plugins." ON)
option (USE_HEVC_DECODER "Build MSDK decoder plugin with HEVC support." ON)
option (USE_HEVC_10BIT_DECODER "Build MSDK decoder plugin with HEVC 10bit support." OFF)
option (USE_VP8_DECODER "Build MSDK decoder plugin with VP8 support." ON)
option (USE_VP9_DECODER "Build MSDK decoder plugin with VP9 support." OFF)

option (MFX_ENCODER "Build MSDK encoder plugin." ON)
CMAKE_DEPENDENT_OPTION(MFX_H264_ENCODER  "Build H.264 encoder plugin" ON
    "MFX_ENCODER" OFF)
CMAKE_DEPENDENT_OPTION(MFX_H265_ENCODER  "Build H.265 encoder plugin" ON
    "MFX_ENCODER" OFF)
CMAKE_DEPENDENT_OPTION(MFX_MPEG2_ENCODER "Build MPEG2 encoder plugin" ON
    "MFX_ENCODER" OFF)
CMAKE_DEPENDENT_OPTION(MFX_JPEG_ENCODER  "Build JPEG encoder plugin"  ON
    "MFX_ENCODER" OFF)

option (MFX_VPP "Build MSDK VPP plugin." ON)

option (MFX_SINK "Build MSDK sink plugin." ON)

CMAKE_DEPENDENT_OPTION(WITH_WAYLAND "Enable Wayland support"
    ON "MFX_SINK" OFF)
CMAKE_DEPENDENT_OPTION(WITH_X11  "Enable X11 support"
    ON "MFX_SINK" OFF)

CMAKE_DEPENDENT_OPTION(USE_WAYLAND_RENDERER "Build sink plugin with Wayland backend"
    ON "WITH_WAYLAND" OFF)
CMAKE_DEPENDENT_OPTION(USE_DRI3_RENDERER  "Build sink plugin with X11 DRI3 backend"
    ON "WITH_X11" OFF)
CMAKE_DEPENDENT_OPTION(USE_EGL_RENDERER "Build sink plugin with EGL backend"
    ON "MFX_SINK" OFF)

CMAKE_DEPENDENT_OPTION (MFX_SINK_BIN "Build MSDK sinkbin plugin."
    ON "MFX_SINK;MFX_VPP" OFF)

option (WITH_MSS_2016 "Build plugins for MSS 2016." OFF)

option (MFX_VC1_PARSER "Build VC1 parser plugin" ON)

include(${CMAKE_SOURCE_DIR}/cmake/ProjectInfo.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/ProjectConfig.cmake)

if (DEBUG)
    set(CMAKE_BUILD_TYPE debug)
else()
    set(CMAKE_BUILD_TYPE release)
endif()

set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}")

configure_file (
    "${PROJECT_SOURCE_DIR}/version.h.in"
    "${PROJECT_BINARY_DIR}/version.h"
    )

include_directories(${PROJECT_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -std=gnu99 -Wall -fPIE")
set(CMAKE_C_RELEASE_FLAGS "${CMAKE_C_RELEASE_FLAGS} -D_FORTIFY_SOURCE=2")

include_directories(
    gst/mfx
    gst-libs/mfx
    parsers)

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "${GSTREAMER_LIBDIR}/gstreamer-1.0")
endif()

SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE) 

SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

#Add gst and gst-libs as subdirectory
add_subdirectory (gst-libs)
add_subdirectory (gst)
add_subdirectory (parsers)

LIST(APPEND SOURCE ${GST_SOURCE})
LIST(APPEND SOURCE ${GST_LIBS_SOURCE})
LIST(APPEND SOURCE ${GST_PARSE})

add_library(gstmfx SHARED ${SOURCE})
target_link_libraries(gstmfx
    ${BASE_LIBRARIES}
    ${SINK_BACKEND}
    ${PARSER}
    stdc++
    libmfx)

# Add uninstall target. Taken from the KDE4 scripts
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in" "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake" @ONLY)
add_custom_target(uninstall "${CMAKE_COMMAND}" -P "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake")

install (TARGETS gstmfx
    LIBRARY DESTINATION lib/gstreamer-1.0
    RUNTIME DESTINATION bin)

message("Build: " ${CMAKE_BUILD_TYPE})
