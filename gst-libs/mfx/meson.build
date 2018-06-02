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
    'gstmfxd3d11device.c',
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
  sources += 'gstmfxencoder.c'
  if not gst_dep.version().version_compare('>=1.15')
    sources += 'common/gstbitwriter.c'
  endif
  encoders = [
    ['MFX_H264_ENCODER', '-DMFX_H264_ENCODER', ['gstmfxencoder_h264.c']],
    ['MFX_H265_ENCODER', '-DMFX_H265_ENCODER', ['gstmfxencoder_h265.c']],
    ['MFX_MPEG2_ENCODER', '-DMFX_MPEG2_ENCODER', ['gstmfxencoder_mpeg2.c']],
    ['MFX_JPEG_ENCODER', '-DMFX_JPEG_ENCODER', ['gstmfxencoder_jpeg.c']]
  ]
  foreach e: encoders
    if get_option (e.get(0))
      gst_mfx_args += [e.get(1)]
      sources += e.get(2)
    endif
  endforeach
endif

gstmfx_libs_tag = static_library('gstmfx-@0@'.format(api_version),
  sources,
  c_args : gst_mfx_args,
  include_directories : config_inc,
  dependencies : gst_mfx_deps,
  pic : true
)

gstmfx_libs_dep = declare_dependency(link_with: gstmfx_libs_tag,
  include_directories : [config_inc, include_directories('.')])
