set(SOURCE 
  "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfx.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxpluginbase.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxpluginutil.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxvideobufferpool.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxvideocontext.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxvideomemory.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxvideometa.c")
	
if(MFX_DECODER)
  list(APPEND SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxdec.c")
endif()

if(MFX_SINK)
  list(APPEND SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxsink.c")
endif()

if(MFX_VPP)
  list(APPEND SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxpostproc.c")
endif()

if(MFX_ENCODER)
  list(APPEND SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxenc.c")
endif()

if(MFX_H264_ENCODER)
  list(APPEND SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxenc_h264.c")
endif()

if(MFX_H265_ENCODER)
  list(APPEND SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxenc_h265.c")
endif()

if(MFX_MPEG2_ENCODER)
  list(APPEND SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxenc_mpeg2.c")
endif()

if(MFX_JPEG_ENCODER)
  list(APPEND SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxenc_jpeg.c")
endif()

if(MFX_SINK_BIN)
list(APPEND SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxsinkbin.c")
endif()

set(GST_SOURCE ${SOURCE} PARENT_SCOPE)
