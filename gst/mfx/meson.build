gstmfx_sources = [
  'gstmfx.c',
  'gstmfxpluginbase.c',
  'gstmfxpluginutil.c',
  'gstmfxvideobufferpool.c',
  'gstmfxvideocontext.c',
  'gstmfxvideomemory.c',
  'gstmfxvideometa.c',
]

gstmfx_dependencies = [
  gstmfx_dep,
  glib_deps,
  gstvideo_dep,
  gstbase_dep,
]

if get_option('MFX_DECODER')
  gstmfx_sources += 'gstmfxdec.c'
endif

if get_option('MFX_ENCODER')
  gstmfx_sources += 'gstmfxenc.c'
  if get_option('MFX_H264_ENCODER')
    gstmfx_sources += 'gstmfxenc_h264.c'
  endif
  if get_option('MFX_H265_ENCODER')
    gstmfx_sources += 'gstmfxenc_h265.c'
  endif
  if get_option('MFX_MPEG2_ENCODER')
    gstmfx_sources += 'gstmfxenc_mpeg2.c'
  endif
  if get_option('MFX_JPEG_ENCODER')
    gstmfx_sources += 'gstmfxenc_jpeg.c'
  endif
endif

if get_option('MFX_SINK')
  gstmfx_sources += 'gstmfxsink.c'
endif

if get_option('MFX_VPP')
  gstmfx_sources += 'gstmfxpostproc.c'
endif

if get_option('MFX_VC1_PARSER')
  gstmfx_dependencies += gstmfxvc1parse_dep
endif

gstmfx = library('gst_mfx',
  gstmfx_sources,
  c_args : gst_mfx_args,
  include_directories: [configinc, libsinc],
  dependencies : gstmfx_dependencies,
  install : true,
  install_dir : plugins_install_dir,
)
