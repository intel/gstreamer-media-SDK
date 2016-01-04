cmake_minimum_required (VERSION 2.8)

# This project use C source code
project (GstMfx C)

set(GstMfx_VERSION_MAJOR 0)
set(GstMfx_VERSION_MINOR 1)


option (DEBUG "Turn on debug build." OFF)
option (HAVE_MFX_DECODER "Build MSDK Decoder plugin." ON)
option (HAVE_MFX_ENCODER "Build MSDK Encoder plugin." OFF)
option (HAVE_MFX_VPP "Build MSDK VPP plugin." OFF)
option (HAVE_MFX_SINK "Build MSDK SINK plugin for Decoder and VPP plugin." OFF)
set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/lib")
message( STATUS "[DEBUG-OUTPUT]: ${LIBRARY_OUTPUT_PATH}")
#Add gst and gst-libs as subdirectory
add_subdirectory (gst-libs)
add_subdirectory (gst)

include_directories(
	gst
	gst-libs
	${CMAKE_SOURCE_DIR}
	)

if (UNIX)
	INCLUDE(CheckFunctionExists)
	INCLUDE(CheckSymbolExists)
	INCLUDE(FindPkgConfig)
endif()

if (DEBUG)
	set (CMAKE_BUILD_TYPE "Debug")
	MESSAGE( STATUS "Build Type: Debug")
endif()

MESSAGE( STATUS "[DEBUG]: ${GST_SOURCE}" )
MESSAGE( STATUS "[DEBUG]: ${GST_LIBS_SOURCE}" )
LIST(APPEND SOURCE ${GST_SOURCE} ${GST_LIBS_SOURCE} )
MESSAGE( STATUS "[GST-SOURCE-DEBUG]: ${SOURCE}" )
pkg_check_modules (GLIB-2.0 REQUIRED glib-2.0)
pkg_check_modules (GOBJECT-2.0 REQUIRED gobject-2.0)
pkg_check_modules (GIO-2.0 REQUIRED gio-2.0)
pkg_check_modules (GSTREAMER REQUIRED gstreamer-1.0>=1.4)
pkg_check_modules (GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0>=1.4)
pkg_check_modules (X11 REQUIRED x11)
pkg_check_modules (LIBVA REQUIRED libva)
pkg_check_modules (LIBVA_X11 REQUIRED libva-x11)
pkg_check_modules (LIBVA_DRM REQUIRED libva-drm)
pkg_check_modules (WAYLAND_CLIENT REQUIRED wayland-client)

#set(COMPILE_CFLAGS ${GLIB-2.0_CFLAGS})
#list(APPEND COMPILE_CFLAGS ${GOBJECT-2.0_CFLAGS} ${GSTREAMER_CFLAGS} ${GSTREAMER_VIDEO_CFLAGS} ${LIBVA_CFLAGS} 
#	${X11_CFLAGS} ${LIBVA_X11_CFLAGS} ${LIBVA_DRM_CFLAGS})
#MESSAGE( STATUS "${COMPILE_CFLAGS}")
include_directories(
	${GLIB-2.0_INCLUDE_DIRS}
	${GOBJECT-2.0_INCLUDE_DIRS}
	${GSTREAMER_INCLUDE_DIR}
	${GSTREAMER_VIDEO_INCLUDE_DIRS}
	${LIBVA_INCLUDE_DIRS}
	${LIBVA_DRM_INCLUDE_DIRS}
	${LIBVA_X11_INLCUDE_DIRS}
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

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -std=c99 -fPIC -lstdc++")
set(MFX_INCLUDES "${MFX_HOME}/include")
set(MFX_LIBRARY  "${MFX_HOME}/lib/lin_x64/libmfx.a")
MESSAGE( STATUS "MFX_INCLUDES=${MFX_INCLUDES}")
MESSAGE( STATUS "MFX_LIBRARY=${MFX_LIBRARY}")

#Add MFX library
add_library(libmfx STATIC IMPORTED)
set_property(TARGET libmfx PROPERTY IMPORTED_LOCATION ${MFX_LIBRARY})
add_definitions(-DHAVE_MFX_DECODER -DHAVE_CONFIG_H)

include_directories( ${MFX_INCLUDES} )
add_library(gstmfx SHARED ${SOURCE})
target_link_libraries(gstmfx
	${GLIB-2.0_LIBRARIES}
	${GOBJECT-2.0_LIBRARIES}
	${GSTREAMER_LIBRARIES}
	${GSTREAMER_VIDEO_LIBRARIES}
	${LIBVA_LIBRARIES}
	${LIBVA_DRM_LIBRARIES}
	${LIBVA_X11_LIBRARIES}
	libmfx
	)
#target_compile_options(libgstmfx PUBLIC ${COMPILE_CFLAGS})
