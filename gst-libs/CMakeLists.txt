set(SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxdisplay.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxfilter.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxminiobject.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxobject.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxprimebufferproxy.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxprofile.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxsurfacepool.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxsurfaceproxy.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxtaskaggregator.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxtask.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxtexture.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxutils_vaapi.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxvalue.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxwindow.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mfx/video-format.c")

if(MFX_DECODER)
    set(SOURCE ${SOURCE}
        "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxdecoder.c")
endif()

if(WAYLAND_RENDERER)
    set(SOURCE ${SOURCE}
        "${CMAKE_CURRENT_SOURCE_DIR}/mfx/wayland/gstmfxdisplay_wayland.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/mfx/wayland/gstmfxwindow_wayland.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/mfx/wayland/wayland-drm-protocol.c")
endif()

if(X11_RENDERER)
    set(SOURCE ${SOURCE}
        "${CMAKE_CURRENT_SOURCE_DIR}/mfx/x11/gstmfxdisplay_x11.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/mfx/x11/gstmfxutils_x11.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/mfx/x11/gstmfxwindow_x11.c")
endif()


if(EGL_RENDERER)
    set(SOURCE ${SOURCE}
        "${CMAKE_CURRENT_SOURCE_DIR}/mfx/egl/gstmfxdisplay_egl.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/mfx/egl/gstmfxtexture_egl.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/mfx/egl/gstmfxwindow_egl.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/mfx/egl/gstmfxutils_egl.c")
endif()

if(MFX_ENCODER)
    set(SOURCE ${SOURCE}
        "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxencoder.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/mfx/common/gstbitwriter.c")
endif()

if(MFX_H264_ENCODER)
    set(SOURCE ${SOURCE}
        "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxencoder_h264.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxutils_h264.c")
endif()

if(MFX_H265_ENCODER)
    set(SOURCE ${SOURCE}
        "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxencoder_h265.c")
endif()
if(MFX_MPEG2_ENCODER)
    set(SOURCE ${SOURCE}
        "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxencoder_mpeg2.c")
endif()
if(MFX_JPEG_ENCODER)
    set(SOURCE ${SOURCE}
        "${CMAKE_CURRENT_SOURCE_DIR}/mfx/gstmfxencoder_jpeg.c")
endif()

set(GST_LIBS_SOURCE ${SOURCE} PARENT_SCOPE)
