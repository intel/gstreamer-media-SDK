set(sources 
"gstmfxcontext.c"
"gstmfxfilter.c" 
"gstmfxprofile.c" 
"gstmfxsurfacepool.c" 
"gstmfxsurface.c" 
"gstmfxtaskaggregator.c" 
"gstmfxtask.c" 
"gstmfxvalue.c" 
"gstmfxwindow.c" 
"video-format.c" 
"gstmfxcompositefilter.c" 
"gstmfxsurfacecomposition.c"
)

if (${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
    list(APPEND gst_mfx_deps "dxgi" "dxguid")
    list(APPEND sources 
    "gstmfxallocator_d3d11.c" 
    "gstmfxsurface_d3d11.c" 
    "gstmfxd3d11device.c"
    )
else()
    list(APPEND sources 
    "gstmfxdisplay.c" 
    "gstmfxprimebufferproxy.c" 
    "gstmfxsurface_vaapi.c"
    "gstmfxallocator_vaapi.c"
    "gstmfxutils_vaapi.c"
    )
endif()

if(mfx_decoder)
    list(APPEND sources "gstmfxdecoder.c")
endif()

if(with_wayland)
    list(APPEND sources 
    "gstmfxdecoder.c"
    "wayland/gstmfxdisplay_wayland.c"
    "wayland/gstmfxwindow_wayland.c"
    "wayland/wayland-drm-protocol.c"
    "wayland/scaler-protocol.c"
    )
endif()

if(with_x11)
    list(APPEND sources 
    "x11/gstmfxdisplay_x11.c"
    "x11/gstmfxutils_x11.c"
    "x11/gstmfxwindow_x11.c"
)
endif()

if(with_d3d11)
    list(APPEND sources "d3d11/gstmfxwindow_d3d11.c")
endif()

if(mfx_encoder)
    list(APPEND sources "gstmfxencoder.c")
    list(APPEND sources "common/gstbitwriter.c")
    if (MFX_H264_ENCODER)
        list(APPEND gst_mfx_args "-DMFX_H264_ENCODER")
        list(APPEND sources "gstmfxencoder_h264.c")
    endif()
    if (MFX_H265_ENCODER)
        list(APPEND gst_mfx_args "-DMFX_H265_ENCODER")
        list(APPEND sources "gstmfxencoder_h265.c")
    endif()   
    if (MFX_MPEG2_ENCODER)
        list(APPEND gst_mfx_args "-DMFX_MPEG2_ENCODER")
        list(APPEND sources "gstmfxencoder_mpeg2.c")
    endif()
    if (MFX_JPEG_ENCODER)
        list(APPEND gst_mfx_args "-DMFX_JPEG_ENCODER")
        list(APPEND sources "gstmfxencoder_jpeg.c")
    endif()
endif()

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR})
add_compile_options(${gst_mfx_args})

ADD_LIBRARY(gstmfx-${api_version} STATIC ${sources})
target_link_libraries(gstmfx-${api_version} ${gst_mfx_deps})
