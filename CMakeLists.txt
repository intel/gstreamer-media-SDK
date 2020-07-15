cmake_minimum_required (VERSION 3.1)
project (gst_mfx VERSION "2.0.2" LANGUAGES "C" "CXX" )
add_compile_options("/W1")

find_package(PkgConfig REQUIRED)

include(${CMAKE_SOURCE_DIR}/cmake/FindVC1ParserDependencies.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/FindBaseDependencies.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/FindMediaSDK.cmake)

option(MFX_DECODER ""  ON)
option(MFX_VP9_DECODER "" OFF)

option(MFX_ENCODER "" ON)
option(MFX_H264_ENCODER ""  ON)
option(MFX_H265_ENCODER "" OFF)
option(MFX_MPEG2_ENCODER "" OFF)
option(MFX_JPEG_ENCODER ""  ON)

option(MFX_VPP ""  ON)
option(MFX_SINK ""  ON)
option(MFX_SINK_BIN ""  ON)

option(USE_DRI3_RENDERER ""  ON)
option(USE_WAYLAND_RENDERER ""  ON)
option(USE_D3D11_RENDERER ""  ON)

set(gst_version ${PROJECT_VERSION})
set(gst_version_major ${PROJECT_VERSION_MAJOR})
set(gst_version_minor ${PROJECT_VERSION_MINOR})
set(gst_version_micro ${PROJECT_VERSION_PATCH})
set(gst_version_nano ${PROJECT_VERSION_TWEAK})

set(glib_req ">= 2.40.0")
set(gst_req ${gst_version_major}.${gst_version_minor})
set(api_version "1.0")
set(temp_var)
MATH(EXPR temp_var "${gst_version_minor}*100+${gst_version_micro}")
set(libversion 0.${temp_var}.0)

set(prefix "" CACHE STRING "")
set(libdir "" CACHE STRING "")
set(plugins_install_dir "${prefix}/${libdir}/gstreamer-1.0")

if (${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
    add_compile_options("/wd4244")
    list(APPEND noseh_link_args "")
else()
    list(APPEND noseh_link_args)
endif()

set(GETTEXT_PACKAGE "gst-mfx")
set(PACKAGE "gst_mfx")
set(PACKAGE_VERSION "${gst_version}")
set(GST_PACKAGE_NAME "GStreamer MFX")

list(APPEND gst_mfx_args)
list(APPEND gst_mfx_deps)

FindVC1(gst_mfx_deps)
FindBaseLibs(gst_mfx_deps)


set(with_pbutils OFF)
if(GSTREAMER_PBUTILS_FOUND)
    set(with_pbutils ON)
endif()

FindMediaSDK()

list(APPEND gst_mfx_deps "legacy_stdio_definitions")

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    list(APPEND gst_mfx_deps "d3d11" "gdi32" "shcore" "stdc++")
    list(APPEND gst_mfx_args "-DWITH_D3D11_BACKEND")
endif()

if(GSTREAMER_GL_FOUND)
    if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        list(APPEND gst_mfx_deps "opengl32")
        list(APPEND gst_mfx_args "-DHAVE_GST_GL_LIBS")
        list(APPEND gst_mfx_args "-DGST_USE_UNSTABLE_API")
    endif()
endif()

set(mfx_decoder ${MFX_DECODER})
if(mfx_decoder)
    list(APPEND gst_mfx_args "-DMFX_DECODER")
endif()

set(mfx_vp9_decoder ${MFX_VP9_DECODER})
if(mfx_vp9_decoder)
    list(APPEND gst_mfx_args "-DMFX_VP9_DECODER")
endif()

if(MFX_VPP)
    list(APPEND gst_mfx_args "-DMFX_VPP")
endif()

set(mfx_sink "${MFX_SINK}")
set(with_wayland OFF)
set(with_x11 OFF)
set(with_d3d11 OFF)
if(mfx_sink)
    if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        if(USE_D3D11_RENDERER)
            list(APPEND gst_mfx_args "-DMFX_SINK")
            set(with_d3d11 ON)
        else()
            set(with_d3d11 OFF)
        endif()
    endif()
else()
endif()

set(mfx_encoder ${MFX_ENCODER})

configure_file(version.h.in version.h)

add_subdirectory (gst-libs)
add_subdirectory (gst)

