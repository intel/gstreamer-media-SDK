if get_option ('MFX_VC1_PARSER')
  if with_codecparsers and with_pbutils
    sources = [
      'gstvc1parse.c',
    ]

  gstmfxvc1parse = static_library('gst-mfx-vc1parse-@0@'.format(api_version),
    sources,
    c_args : gst_mfx_args,
    include_directories: include_directories('.'),
    version : libversion,
    dependencies : [glib_deps, gst_dep, gstbase_dep, mfx_dep, gstvideo_dep, gstcodecparsers_dep, gstpbutils_dep],
  )

  gstmfxvc1parse_dep = declare_dependency(link_with: gstmfxvc1parse,
    include_directories : include_directories('.'),
    dependencies : [gst_dep, mfx_dep, gstcodecparsers_dep, gstpbutils_dep])

  gst_mfx_args += ['-DMFX_VC1_PARSER']
  gst_mfx_deps += [gstmfxvc1parse_dep]

  elif get_option ('MFX_VC1_PARSER')
    error ('MFX_VC1_PARSER required, but pbutils or codecparsers are not present')
  endif
endif