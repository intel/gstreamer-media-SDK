if get_option('MFX_VC1_PARSER')

  gstmfxvc1parse_sources = [
    'gstvc1parse.c',
  ]
  
  gstmfxvc1parse = library('gst-mfx-vc1parse-@0@'.format(api_version),
    gstmfxvc1parse_sources,
    c_args : gst_mfx_args,
    include_directories: [configinc, libsinc],
    version : libversion,
    soversion : soversion,
    install : true,
    dependencies : [glib_deps, gst_dep, gstbase_dep, mfx_dep, gstvideo_dep, gstcodecparsers_dep, gstpbutils_dep],
    vs_module_defs: vs_module_defs_dir + 'libgstmfxvc1parse.def',
  )
  	
  gstmfxvc1parse_dep = declare_dependency(link_with: gstmfxvc1parse,
    include_directories : [libsinc],
    dependencies : [gst_dep, mfx_dep, gstcodecparsers_dep, gstpbutils_dep],
    sources : gstmfxvc1parse_sources)
  
endif