sources = [
  'gstmfxcontext.c',
  'gstmfxfilter.c',
  'gstmfxprofile.c',
  'gstmfxsurfacepool.c',
  'gstmfxsurface.c',
  'gstmfxtaskaggregator.c',
  'gstmfxtask.c',
  'gstmfxvalue.c',
  'gstmfxwindow.c',
  'video-format.c',
  'gstmfxcompositefilter.c',
  'gstmfxsurfacecomposition.c'
]

if host_machine.system() == 'windows'
  gst_mfx_deps += cc.find_library('dxgi')
  gst_mfx_deps += cc.find_library('dxguid')

  sources += [
    'gstmfxallocator_d3d11.c',
    'gstmfxsurface_d3d11.c',
    'd3d11/gstmfxdevice.c',
  ]
else
  sources += [
    'gstmfxdisplay.c',
    'gstmfxprimebufferproxy.c',
    'gstmfxsurface_vaapi.c',
	'gstmfxallocator_vaapi.c',
    'gstmfxutils_vaapi.c',
  ]
endif
  
if mfx_decoder
  sources += 'gstmfxdecoder.c'
endif

if with_wayland
  sources += [
    'wayland/gstmfxdisplay_wayland.c',
    'wayland/gstmfxwindow_wayland.c',
    'wayland/wayland-drm-protocol.c',
    'wayland/scaler-protocol.c',
  ]
endif

if with_x11
  sources += [
    'x11/gstmfxdisplay_x11.c',
    'x11/gstmfxutils_x11.c',
    'x11/gstmfxwindow_x11.c',
  ]
endif

if with_d3d11
  sources += 'd3d11/gstmfxwindow_d3d11.c'
endif

if mfx_encoder
  sources += [
    'gstmfxencoder.c',
    'common/gstbitwriter.c'
  ]
  encoders = [
    ['MFX_H264_ENCODER', '-DMFX_H264_ENCODER', ['gstmfxencoder_h264.c', 'gstmfxutils_h264.c']],
    ['MFX_H265_ENCODER', '-DMFX_H265_ENCODER', ['gstmfxencoder_h265.c']],
    ['MFX_MPEG2_ENCODER', '-DMFX_MPEG2_ENCODER', ['gstmfxencoder_mpeg2.c']],
    ['MFX_JPEG_ENCODER', '-DMFX_JPEG_ENCODER', ['gstmfxencoder_jpeg.c']]
  ]
  foreach e: encoders
    if get_option (e.get(0)) != 'no'
      gst_mfx_args += [e.get(1)]
      sources += e.get(2)
    endif
  endforeach
endif


foreach s: sources
  gst_mfx_sources += ['@0@/@1@'.format(meson.current_source_dir(), s)]
endforeach
  
