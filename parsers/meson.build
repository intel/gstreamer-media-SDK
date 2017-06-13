if get_option ('MFX_VC1_PARSER') != 'no'
	if with_codecparsers and with_pbutils
		mfx_deps += [gstcodecparsers_dep, gstpbutils_dep]
		mfx_sources += ['@0@/@1@'.format(meson.current_source_dir(), 'gstvc1parse.c')]
	elif get_option ('MFX_VC1_PARSER') == 'yes'
		error ('MFX_VC1_PARSER required, but pbutils or codecparsers are not present')
	endif
endif
