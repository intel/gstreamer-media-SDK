sources = ['mfx/gstmfxdisplay.c',
	'mfx/gstmfxfilter.c',
	'mfx/gstmfxminiobject.c',
	'mfx/gstmfxprimebufferproxy.c',
	'mfx/gstmfxprofile.c',
	'mfx/gstmfxsurfacepool.c',
	'mfx/gstmfxsurface.c',
	'mfx/gstmfxsurface_vaapi.c',
	'mfx/gstmfxtaskaggregator.c',
	'mfx/gstmfxtask.c',
	'mfx/gstmfxutils_vaapi.c',
	'mfx/gstmfxvalue.c',
	'mfx/gstmfxwindow.c',
	'mfx/video-format.c',
	'mfx/gstmfxcompositefilter.c',
	'mfx/gstmfxsurfacecomposition.c'
	]

if mfx_decoder
	sources += ['mfx/gstmfxdecoder.c']
endif

if with_wayland
	sources += ['mfx/wayland/gstmfxdisplay_wayland.c',
			'mfx/wayland/gstmfxwindow_wayland.c',
			'mfx/wayland/wayland-drm-protocol.c',
			'mfx/wayland/scaler-protocol.c']
endif

if with_x11
	sources += ['mfx/x11/gstmfxdisplay_x11.c',
			'mfx/x11/gstmfxutils_x11.c',
			'mfx/x11/gstmfxwindow_x11.c']
endif

if use_egl_renderer
	sources += ['mfx/egl/gstmfxdisplay_egl.c',
			'mfx/egl/gstmfxtexture_egl.c',
			'mfx/egl/gstmfxwindow_egl.c',
			'mfx/egl/gstmfxutils_egl.c']
endif

if mfx_encoder
	sources += ['mfx/gstmfxencoder.c',
			'mfx/common/gstbitwriter.c']
	encoders = [
		['MFX_H264_ENCODER', '-DMFX_H264_ENCODER', ['mfx/gstmfxencoder_h264.c', 'mfx/gstmfxutils_h264.c']],
		['MFX_H265_ENCODER', '-DMFX_H265_ENCODER', ['mfx/gstmfxencoder_h265.c']],
		['MFX_MPEG2_ENCODER', '-DMFX_MPEG2_ENCODER', ['mfx/gstmfxencoder_mpeg2.c']],
		['MFX_JPEG_ENCODER', '-DMFX_JPEG_ENCODER', ['mfx/gstmfxencoder_jpeg.c']]
	]
	foreach e: encoders
		if get_option (e.get(0)) != 'no'
			mfx_c_args += [e.get(1)]
			sources += e.get(2)
		endif
	endforeach
endif


foreach s: sources
	mfx_sources += ['@0@/@1@'.format(meson.current_source_dir(), s)]
endforeach
