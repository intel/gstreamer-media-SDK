sources = [
  'gstmfx.c',
  'gstmfxpluginbase.c',
  'gstmfxpluginutil.c',
  'gstmfxvideobufferpool.c',
  'gstmfxvideocontext.c',
  'gstmfxvideomemory.c',
  'gstmfxvideometa.c'
]

if mfx_decoder
  sources += ['gstmfxdec.c']
  if get_option ('USE_VP9_DECODER')
    gst_mfx_args += '-DUSE_VP9_DECODER'
  endif
else
  if get_option ('USE_VP9_DECODER')
    error('USE_VP9_DECODER required but MFX_DECODER is false')
  endif
endif

if mfx_sink
  sources += 'gstmfxsink.c'
endif

mfx_vpp = get_option('MFX_VPP')
if mfx_vpp
  sources += 'gstmfxpostproc.c'
  gst_mfx_args += '-DMFX_VPP'
endif

if mfx_sink and mfx_vpp and with_pbutils
  if get_option('MFX_SINK_BIN')
    sources += 'gstmfxsinkbin.c'
    gst_mfx_args += '-DMFX_SINK_BIN'
  endif
elif get_option('MFX_SINK_BIN')
  error('MFX_SINK_BIN required but MFX_SINK or MFX_VPP is false')
endif

if mfx_encoder
  sources += 'gstmfxenc.c'
  encoders = [
    ['MFX_H264_ENCODER', '-DMFX_H264_ENCODER', 'gstmfxenc_h264.c'],
    ['MFX_H265_ENCODER', '-DMFX_H265_ENCODER', 'gstmfxenc_h265.c'],
    ['MFX_MPEG2_ENCODER', '-DMFX_MPEG2_ENCODER', 'gstmfxenc_mpeg2.c'],
    ['MFX_JPEG_ENCODER', '-DMFX_JPEG_ENCODER', 'gstmfxenc_jpeg.c']
  ]
  foreach e: encoders
    if get_option (e.get(0))
      gst_mfx_args += [e.get(1)]
      sources += [e.get(2)]
    endif
  endforeach
else
  foreach opt: ['MFX_H264_ENCODER', 'MFX_H265_ENCODER', 'MFX_MPEG2_ENCODER', 'MFX_JPEG_ENCODER']
    if get_option (opt)
      error('@0@ required but MFX_ENCODER is false'.format(opt))
    endif
  endforeach
endif

gstmfxtag = library('gstmfx',
  sources,
  c_args : gst_mfx_args,
  include_directories : include_directories('.'),
  dependencies : [gst_mfx_deps, gstmfx_libs_dep],
  install : true,
  install_dir : plugins_install_dir,
)
