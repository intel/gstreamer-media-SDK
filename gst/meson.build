sources = ['mfx/gstmfx.c',
	'mfx/gstmfxpluginbase.c',
	'mfx/gstmfxpluginutil.c',
	'mfx/gstmfxvideobufferpool.c',
	'mfx/gstmfxvideocontext.c',
	'mfx/gstmfxvideomemory.c',
	'mfx/gstmfxvideometa.c']

if mfx_decoder
	sources += ['mfx/gstmfxdec.c']
	if get_option ('USE_HEVC_DECODER') != 'no'
		mfx_c_args += ['-DUSE_HEVC_DECODER']
	endif
	if get_option ('USE_HEVC_10BIT_DECODER') != 'no'
		mfx_c_args += ['-DUSE_HEVC_10BIT_DECODER']
	endif
	if get_option ('USE_VP8_DECODER') != 'no'
		mfx_c_args += ['-DUSE_VP8_DECODER']
	endif
	if get_option ('USE_VP9_DECODER') != 'no'
		mfx_c_args += ['-DUSE_VP9_DECODER']
	endif
else
	if get_option ('USE_HEVC_DECODER') == 'yes'
		error('USE_HEVC_DECODER required but MFX_DECODER is false')
	endif
	if get_option ('USE_HEVC_10BIT_DECODER') == 'yes'
		error('USE_HEVC_10BIT_DECODER required but MFX_DECODER is false')
	endif
	if get_option ('USE_VP8_DECODER') == 'yes'
		error('USE_VP8_DECODER required but MFX_DECODER is false')
	endif
	if get_option ('USE_VP9_DECODER') == 'yes'
		error('USE_VP9_DECODER required but MFX_DECODER is false')
	endif
endif

if mfx_sink
	sources += ['mfx/gstmfxsink.c']
endif

mfx_vpp = get_option('MFX_VPP')
if mfx_vpp
	sources += ['mfx/gstmfxpostproc.c']
	mfx_c_args += ['-DMFX_VPP']
endif

if mfx_sink and mfx_vpp and with_pbutils
	if get_option('MFX_SINK_BIN') != 'no'
		sources += ['mfx/gstmfxsinkbin.c']
		mfx_c_args += ['-DMFX_SINK_BIN']
	endif
elif get_option('MFX_SINK_BIN') == 'yes'
	error('MFX_SINK_BIN required but MFX_SINK or MFX_VPP is false')
endif

if mfx_encoder
	sources += ['mfx/gstmfxenc.c']
	encoders = [
		['MFX_H264_ENCODER', '-DMFX_H264_ENCODER', 'mfx/gstmfxenc_h264.c'],
		['MFX_H265_ENCODER', '-DMFX_H265_ENCODER', 'mfx/gstmfxenc_h265.c'],
		['MFX_MPEG2_ENCODER', '-DMFX_MPEG2_ENCODER', 'mfx/gstmfxenc_mpeg2.c'],
		['MFX_JPEG_ENCODER', '-DMFX_JPEG_ENCODER', 'mfx/gstmfxenc_jpeg.c']
	]
	foreach e: encoders
		if get_option (e.get(0)) != 'no'
			mfx_c_args += [e.get(1)]
			sources += [e.get(2)]
		endif
	endforeach
else
	foreach opt: ['MFX_H264_ENCODER', 'MFX_H265_ENCODER', 'MFX_MPEG2_ENCODER', 'MFX_JPEG_ENCODER']
		if get_option (opt) == 'yes'
			error('@0@ required but MFX_ENCODER is false'.format(opt))
		endif
	endforeach
endif

foreach s: sources
	mfx_sources += ['@0@/@1@'.format(meson.current_source_dir(), s)]
endforeach
