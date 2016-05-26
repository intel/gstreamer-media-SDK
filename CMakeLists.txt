cmake_minimum_required (VERSION 2.8)

# This project use C source code
project (GstMfx C)

set(GstMfx_VERSION_MAJOR 0)
set(GstMfx_VERSION_MINOR 1)
set(GstMfx_BUILD_NUMBER 0)
string(CONCAT GST_MFX_VERSION ${GstMfx_VERSION_MAJOR} "." ${GstMfx_VERSION_MINOR} "." ${GstMfx_BUILD_NUMBER})

option (DEBUG "Turn on debug build." OFF)

option (HAVE_MFX_DECODER "Build MSDK Decoder plugin." ON)
option (HAVE_MFX_ENCODER "Build MSDK Encoder plugin." OFF)
option (HAVE_MFX_VPP "Build MSDK VPP plugin." ON)
option (HAVE_MFX_SINK "Build MSDK SINK plugin." ON)
option (FOR_MSS "Build plugins for MSS." OFF)

option (HAVE_WAYLAND_RENDERER "Build MSDK SINK plugin with Wayland renderer." ON)
option (HAVE_X11_RENDERER "Build MSDK SINK plugin with X11 renderer." ON)
option (HAVE_EGL_RENDERER "Build MSDK SINK plugin with EGL renderer." ON)

if (UNIX)
	INCLUDE(FindPkgConfig)
endif()

set(HAVE_EGL 0)
set(HAVE_X11 0)
set(HAVE_WAYLAND 0)
set(HAVE_XRANDR 0)
set(HAVE_XKBLIB 0)

if(HAVE_MFX_SINK)
	set(MFX_SINK 1)
else()
	set(MFX_SINK 0)
endif()

if(HAVE_MFX_DECODER)
	set(MFX_DECODE 1)
else()
	set(MFX_DECODE 0)
endif()

if(HAVE_MFX_ENCODE)
	set(MFX_ENCODE 1)
else()
	set(MFX_ENCODE 0)
endif()

if(HAVE_MFX_VPP)
	set(MFX_VPP 1)
else()
	set(MFX_VPP 0)
endif()

if (DEBUG)
    set(CMAKE_BUILD_TYPE debug)
else()
    set(CMAKE_BUILD_TYPE release)
endif()

set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}")

include(cmake/FindSinkDependencies.cmake)
include(cmake/FindMediaSDK.cmake)
include(cmake/FindBaseDependencies.cmake)

configure_file (
    "${PROJECT_SOURCE_DIR}/config.h.in"
    "${PROJECT_BINARY_DIR}/config.h"
    )

include_directories("${PROJECT_BINARY_DIR}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -lstdc++ -DHAVE_CONFIG_H")

if (FOR_MSS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFOR_MSS")
endif()

include_directories(
	gst/mfx
	gst-libs/mfx
	)

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

LIST(APPEND SOURCE ${GST_SOURCE})

add_library(gstmfx SHARED ${SOURCE})
target_link_libraries(gstmfx
    ${BASE_LIBRARIES}
	${SINK_LIBRARIES}
	libmfx
	gstmfxhelper
	)

# Add uninstall target. Taken from the KDE4 scripts
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in" "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake" @ONLY)
add_custom_target(uninstall "${CMAKE_COMMAND}" -P "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake")

install (TARGETS gstmfx
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}
    )

message("Build: " ${CMAKE_BUILD_TYPE})
