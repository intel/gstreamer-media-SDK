additional_deps = []

gst_mfx_libs_sources = [
  'gstmfxdecoder.c',
  'gstmfxencoder.c',
  'gstmfxencoder_h264.c',
  'gstmfxencoder_h265.c',
  'gstmfxencoder_jpeg.c',
  'gstmfxencoder_mpeg2.c',
  'gstmfxfilter.c',
  'gstmfxprofile.c',
  'gstmfxsurface.c',
  'gstmfxsurfacepool.c',
  'gstmfxtask.c',
  'gstmfxtaskaggregator.c',
  'gstmfxutils_h264.c',
  'gstmfxvalue.c',
  'gstmfxwindow.c',
  'video-format.c',
  'common/gstbitwriter.c',
]

if get_option('USE_D3D11_RENDERER')
  additional_deps += cc.find_library('dxgi')
  additional_deps += cc.find_library('d3d11')
  additional_deps += cc.find_library('dxguid')

  gst_mfx_libs_sources += 'd3d11/gstmfxwindow_d3d11.c'
endif

gstmfx = library('gst-mfx-@0@'.format(api_version),
  gst_mfx_libs_sources,
  c_args : gst_mfx_args,
  include_directories: [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  install : true,
  dependencies : [glib_deps, gst_dep, mfx_dep, gstvideo_dep, additional_deps],
  vs_module_defs: vs_module_defs_dir + 'libgstmfx.def',
)
	
gstmfx_dep = declare_dependency(link_with: gstmfx,
  include_directories : [libsinc],
  dependencies : [gst_dep, mfx_dep, additional_deps],
  sources : gst_mfx_libs_sources)
  