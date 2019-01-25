set(sources 
  "gstmfx.c"
  "gstmfxpluginbase.c"
  "gstmfxpluginutil.c"
  "gstmfxvideobufferpool.c"
  "gstmfxvideocontext.c"
  "gstmfxvideomemory.c"
  "gstmfxvideometa.c"
  )

if(mfx_encoder)
  list(APPEND sources "gstmfxdec.c")
endif()

if(mfx_sink)
    list(APPEND sources "gstmfxsink.c")
endif()

set(mfx_vpp ${MFX_VPP})
if(mfx_vpp)
  list(APPEND sources "gstmfxpostproc.c")
endif()

if(mfx_sink AND mfx_vpp AND with_pbutils)
    if(MFX_SINK_BIN)
        list(APPEND sources "gstmfxsinkbin.c")
        list(APPEND gst_mfx_args "-DMFX_SINK_BIN")
    endif()
endif()

if(mfx_encoder)
    list(APPEND sources "gstmfxenc.c")
    if(MFX_H264_ENCODER)
        list(APPEND sources "gstmfxenc_h264.c")
        list(APPEND gst_mfx_args "-DMFX_H264_ENCODER")
    endif()

    if(MFX_H265_ENCODER)
        list(APPEND sources "gstmfxenc_h265.c")
        list(APPEND gst_mfx_args "-DMFX_H265_ENCODER")
    endif()

    if(MFX_MPEG2_ENCODER)
        list(APPEND sources "gstmfxenc_mpeg2.c")
        list(APPEND gst_mfx_args "-DMFX_MPEG2_ENCODER")
     endif()

    if(MFX_JPEG_ENCODER)
        list(APPEND sources "gstmfxenc_jpeg.c")
        list(APPEND gst_mfx_args "-DMFX_JPEG_ENCODER")
    endif()
endif()


include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR}/gst-libs/mfx)
include_directories(${CMAKE_SOURCE_DIR})

add_compile_options(${gst_mfx_args})

ADD_LIBRARY(gstmfx SHARED ${sources})

target_link_libraries(gstmfx ${gst_mfx_deps} libmfx gstmfx-${api_version})
#message(${plugins_install_dir})
install(TARGETS gstmfx DESTINATION ${plugins_install_dir})