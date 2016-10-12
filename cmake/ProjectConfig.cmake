set(BASE_LIBRARIES "")
set(SINK_BACKEND "")
set(PARSER "")

if(UNIX)
  include(FindPkgConfig)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/FindMediaSDK.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/FindBaseDependencies.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/FindSinkDependencies.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/FindVC1ParserDependencies.cmake)

FindMediaSDK()
FindBaseLibs(BASE_LIBRARIES)

add_definitions(-DHAVE_CONFIG_H)

if(WITH_MSS)
  add_definitions(-DWITH_MSS)
endif()

if(MFX_DECODER)
  add_definitions(-DMFX_DECODER)
  if(MFX_VP9_DECODER)
    add_definitions(-DHAS_VP9)
  endif()
endif()

if(MFX_VPP)
  add_definitions(-DMFX_VPP)
endif()

if(MFX_SINK)
  add_definitions(-DMFX_SINK)
  if(WAYLAND_RENDERER)
    FindWayland(SINK_BACKEND)
    add_definitions(-DUSE_WAYLAND)
    if(EGL_RENDERER)
      FindEGL(SINK_BACKEND)
      FindEGLWayland(SINK_BACKEND)
      add_definitions(-DUSE_EGL)
    endif()
  endif()
  
  if(X11_RENDERER)
    FindX11(SINK_BACKEND)
    add_definitions(-DUSE_X11)
    if(EGL_RENDERER)
      FindEGL(SINK_BACKEND)
      add_definitions(-DUSE_EGL)
    endif()
  endif()
endif()

if(MFX_SINK_BIN)
  add_definitions(-DMFX_SINK_BIN)
endif()

if(MFX_H264_ENCODER)
  add_definitions(-DMFX_H264_ENCODER)
endif()

if(MFX_H265_ENCODER)
  add_definitions(-DMFX_H265_ENCODER)
endif()

if(MFX_MPEG2_ENCODER)
  add_definitions(-DMFX_MPEG2_ENCODER)
endif()

if(MFX_JPEG_ENCODER)
  add_definitions(-DMFX_JPEG_ENCODER)
endif()

if(MFX_JPEG_ENCODER)
  add_definitions(-DMFX_JPEG_ENCODER)
endif()

if(MFX_VC1_PARSER)
  FindVC1(PARSER)
  add_definitions(-DMFX_VC1_PARSER)
endif()
