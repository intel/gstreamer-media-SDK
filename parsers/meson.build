if get_option ('MFX_VC1_PARSER')
  if with_codecparsers and with_pbutils
    gst_mfx_args += ['-DMFX_VC1_PARSER']
    gst_mfx_deps += [gstcodecparsers_dep, gstpbutils_dep]
    gst_mfx_sources += ['@0@/@1@'.format(meson.current_source_dir(), 'gstvc1parse.c')]
  elif get_option ('MFX_VC1_PARSER')
    error ('MFX_VC1_PARSER required, but pbutils or codecparsers are not present')
  endif
endif
