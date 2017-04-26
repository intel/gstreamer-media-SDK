gst_mfx_libs_sources = [
  'gstmfxdecoder.c',
  'gstmfxencoder.c',
  'gstmfxencoder_h264.c',
  'gstmfxencoder_h265.c',
  'gstmfxencoder_jpeg.c',
  'gstmfxencoder_mpeg2.c',
  'gstmfxfilter.c',
  'gstmfxminiobject.c',
  'gstmfxprofile.c',
  'gstmfxsurface.c',
  'gstmfxsurfacepool.c',
  'gstmfxtask.c',
  'gstmfxtaskaggregator.c',
  'gstmfxutils_h264.c',
  'gstmfxvalue.c',
  'video-format.c',
  'common/gstbitwriter.c',
]

gstmfx = library('gst-mfx-@0@'.format(api_version),
  gst_mfx_libs_sources,
  c_args : gst_mfx_args,
  include_directories: [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  install : true,
  dependencies : [glib_deps, gst_dep, mfx_dep, gstvideo_dep],
  vs_module_defs: vs_module_defs_dir + 'libgstmfx.def',
)
	
gstmfx_dep = declare_dependency(link_with: gstmfx,
  include_directories : [libsinc],
  dependencies : [gst_dep, mfx_dep],
  sources : gst_mfx_libs_sources)
  