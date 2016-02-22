cmake_minimum_required (VERSION 2.8)

# This project use C source code
project (GstMfx C)

set(GstMfx_VERSION_MAJOR 0)
set(GstMfx_VERSION_MINOR 1)
string(CONCAT GST_MFX_VERSION ${GstMfx_VERSION_MAJOR} "." ${GstMfx_VERSION_MINOR})

option (DEBUG "Turn on debug build." OFF)

option (HAVE_MFX_DECODER "Build MSDK Decoder plugin." ON)
option (HAVE_MFX_ENCODER "Build MSDK Encoder plugin." OFF)
option (HAVE_MFX_VPP "Build MSDK VPP plugin." OFF)
option (HAVE_MFX_SINK "Build MSDK SINK plugin." ON)

option (HAVE_WAYLAND_RENDERER "Build MSDK SINK plugin with Wayland renderer." ON)
option (HAVE_X11_RENDERER "Build MSDK SINK plugin with X11 renderer." OFF)
option (HAVE_EGL_RENDERER "Build MSDK SINK plugin with EGL renderer." OFF)

set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/lib")

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


include(cmake/FindSinkDependencies.cmake)

pkg_check_modules (GLIB-2.0 REQUIRED glib-2.0)
pkg_check_modules (GOBJECT-2.0 REQUIRED gobject-2.0)
pkg_check_modules (GIO-2.0 REQUIRED gio-2.0)
pkg_check_modules (GSTREAMER REQUIRED gstreamer-1.0>=1.4)
pkg_check_modules (GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0>=1.4)
pkg_check_modules (LIBVA REQUIRED libva)
pkg_check_modules (LIBDRM REQUIRED libdrm)
pkg_check_modules (LIBDRM_INTEL REQUIRED libdrm_intel)
pkg_check_modules (LIBVA_DRM REQUIRED libva-drm)
pkg_check_modules (LIBUDEV REQUIRED libudev)

set(GST_API_VERSION ${GSTREAMER_VERSION})

configure_file (
    "${PROJECT_SOURCE_DIR}/config.h.in"
    "${PROJECT_BINARY_DIR}/config.h"
    )

include_directories("${PROJECT_BINARY_DIR}")

include_directories(
	${GLIB-2.0_INCLUDE_DIRS}
	${GOBJECT-2.0_INCLUDE_DIRS}
	${GSTREAMER_INCLUDE_DIR}
	${GSTREAMER_VIDEO_INCLUDE_DIRS}
	${LIBVA_INCLUDE_DIRS}
	${LIBDRM_INCLUDE_DIRS}
	${LIBDRM_INTEL_INCLUDE_DIRS}
	${LIBVA_DRM_INCLUDE_DIRS}
	${LIBUDEV_INCLUDE_DIRS}
	)

if (DEFINED ENV{MFX_HOME})
	MESSAGE( STATUS "MFX_HOME=$ENV{MFX_HOME}")
	set(MFX_HOME $ENV{MFX_HOME})
elseif (EXISTS "/opt/intel/mediasdk")
	MESSAGE( STATUS "MediaSDK distribution found in /opt/intel/mediasdk")
	set(MFX_HOME "/opt/intel/mediasdk")
else()
	MESSAGE( FATAL_ERROR "No MediaSDK distribution is found.")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fPIC -lstdc++")
set(MFX_INCLUDES "${MFX_HOME}/include")
set(MFX_LIBRARY  "${MFX_HOME}/lib/lin_x64/libmfx.a")
MESSAGE( STATUS "MFX_INCLUDES=${MFX_INCLUDES}")
MESSAGE( STATUS "MFX_LIBRARY=${MFX_LIBRARY}")

#Add MFX library
add_library(libmfx STATIC IMPORTED)
set_property(TARGET libmfx PROPERTY IMPORTED_LOCATION ${MFX_LIBRARY})
add_definitions(-DHAVE_MFX_DECODER -DHAVE_CONFIG_H)
include_directories( ${MFX_INCLUDES} )

include_directories(
	gst/mfx
	gst-libs/mfx
	)

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set (CMAKE_INSTALL_PREFIX "/usr/lib64/gstreamer-1.0")
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
	${GLIB-2.0_LIBRARIES}
	${GOBJECT-2.0_LIBRARIES}
	${GSTREAMER_LIBRARIES}
	${GSTREAMER_VIDEO_LIBRARIES}
	${LIBVA_LIBRARIES}
	${LIBVA_DRM_LIBRARIES}
	${LIBDRM_LIBRARIES}
	${LIBDRM_INTEL_LIBRARIES}
	${LIBUDEV_LIBRARIES}
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
