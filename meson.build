project('gst_mfx', 'c', 'cpp',
  version : '1.5.0',
  meson_version : '>= 0.36.0',
  default_options : [ 'warning_level=1',
                      'buildtype=debugoptimized' ])

gst_version = meson.project_version()
version_arr = gst_version.split('.')
gst_version_major = version_arr[0]
gst_version_minor = version_arr[1]
gst_version_micro = version_arr[2]
if version_arr.length() == 4
  gst_version_nano = version_arr[3]
else
  gst_version_nano = 0
endif

glib_req = '>= 2.40.0'
gst_req = '>= @0@.@1@.0'.format(gst_version_major, gst_version_minor)

api_version = '1.0'
soversion = 0
# maintaining compatibility with the previous libtool versioning
# current = minor * 100 + micro
# FIXME: should be able to convert string to int somehow so we can just do maths
libversion = '@0@.@1@.0'.format(soversion, gst_version_minor.to_int() * 100 + gst_version_micro.to_int())

plugins_install_dir = '@0@/gstreamer-1.0'.format(get_option('libdir'))

cc = meson.get_compiler('c')

if cc.get_id() == 'msvc'
  # Ignore several spurious warnings for things gstreamer does very commonly
  # If a warning is completely useless and spammy, use '/wdXXXX' to suppress it
  # If a warning is harmless but hard to fix, use '/woXXXX' so it's shown once
  # NOTE: Only add warnings here if you are sure they're spurious
  add_project_arguments(
      '/wd4018', # implicit signed/unsigned conversion
      '/wd4146', # unary minus on unsigned (beware INT_MIN)
      '/wd4244', # lossy type conversion (e.g. double -> int)
      '/wd4305', # truncating type conversion (e.g. double -> float)
      language : 'c')
  # Disable SAFESEH with MSVC for plugins and libs that use external deps that
  # are built with MinGW
  noseh_link_args = ['/SAFESEH:NO']
else
  noseh_link_args = []
endif

core_conf = configuration_data()

core_conf.set('GETTEXT_PACKAGE', 'gstreamer-msdk')
core_conf.set('PACKAGE', 'gst_mfx')
core_conf.set('VERSION', '@0@'.format(gst_version))
core_conf.set('PACKAGE_VERSION', '@0@'.format(gst_version))
core_conf.set('GST_PACKAGE_NAME', 'GStreamer Media SDK')
core_conf.set('GST_PACKAGE_ORIGIN', 'Unknown package origin')
core_conf.set('GST_API_VERSION', '@0@'.format(api_version))
core_conf.set('GST_INSTALL_PLUGINS_HELPER', '/FIXME')
core_conf.set('GST_DATADIR', '/FIXME')
core_conf.set('GST_LICENSE', 'LGPL')


gst_mfx_args = ['-DHAVE_CONFIG_H']

if get_option('MFX_DECODER')
  gst_mfx_args += ['-DMFX_DECODER']
  if get_option('USE_HEVC_DECODER')
    gst_mfx_args += ['-DUSE_HEVC_DECODER']
  endif
  if get_option('USE_VP8_DECODER')
    gst_mfx_args += ['-DUSE_VP8_DECODER']
  endif
  if get_option('USE_VP9_DECODER')
    gst_mfx_args += ['-DUSE_VP9_DECODER']
  endif
endif

if get_option('MFX_ENCODER')
  gst_mfx_args += ['-DMFX_ENCODER']
  if get_option('MFX_H264_ENCODER')
    gst_mfx_args += ['-DMFX_H264_ENCODER']
  endif
  if get_option('MFX_H265_ENCODER')
    gst_mfx_args += ['-DMFX_H265_ENCODER']
  endif
  if get_option('MFX_MPEG2_ENCODER')
    gst_mfx_args += ['-DMFX_MPEG2_ENCODER']
  endif
  if get_option('MFX_JPEG_ENCODER')
    gst_mfx_args += ['-DMFX_JPEG_ENCODER']
  endif
endif

if get_option('MFX_VPP')
  gst_mfx_args += ['-DMFX_VPP']
endif

if get_option('MFX_SINK')
  gst_mfx_args += ['-DMFX_SINK']
endif

if get_option('MFX_SINK_BIN')
  gst_mfx_args += ['-DMFX_SINK_BIN']
endif

if get_option('MFX_VC1_PARSER')
  gst_mfx_args += ['-DMFX_VC1_PARSER']
endif

configinc = include_directories('.')
libsinc = include_directories('gst-libs/mfx')

vs_module_defs_dir = meson.current_source_dir() + '/win32/common/'


# To use the subproject make subprojects directory
# and put gstreamer meson git there (symlinking is fine)
# Mandatory GST deps
glib_deps = [dependency('glib-2.0', version : glib_req),
     dependency('gobject-2.0', version : glib_req)]
gst_dep = dependency('gstreamer-1.0', version : gst_req,
  fallback : ['gstreamer', 'gst_dep'])
gstbase_dep = dependency('gstreamer-base-1.0', version : gst_req,
  fallback : ['gstreamer', 'gstbase_dep'])
gstvideo_dep = dependency('gstreamer-video-1.0', version : gst_req,
  fallback : ['gstreamer', 'gstvideo_dep'])
  
if get_option('MFX_VC1_PARSER')
  gstcodecparsers_dep = dependency('gstreamer-codecparsers-1.0', version : gst_req,
    fallback : ['gstreamer', 'gst_dep'])
  gstpbutils_dep = dependency('gstreamer-pbutils-1.0', version : gst_req,
    fallback : ['gstreamer', 'gst_dep'])
endif

#TODO: check if needed: vs_module_defs_dir = meson.current_source_dir() + '/win32/common/'

#if gst_dep.type_name() == 'internal'
#    gst_proj = subproject('gstreamer')
#
#    if gst_proj.get_variable('disable_gst_debug')
#        message('GStreamer debug system is disabled')
#        add_project_arguments('-Wno-unused', language: 'c')
#    else
#        message('GStreamer debug system is enabled')
#    endif
#else
#    # We can't check that in the case of subprojects as we won't
#    # be able to build against an internal dependency (which is not built yet)
#    if not cc.compiles('''
##include <gst/gstconfig.h>
##ifdef GST_DISABLE_GST_DEBUG
##error "debugging disabled, make compiler fail"
##endif''' , dependencies: gst_dep)
#        message('GStreamer debug system is disabled')
#        add_project_arguments('-Wno-unused', language: 'c')
#    else
#        message('GStreamer debug system is enabled')
#    endif
#endif

mfx_home = get_option('MFX_HOME')
if mfx_home == ''
  python = find_program('python')		 
  mfx_home = run_command(python, '-c', 'import os; print(os.environ.get("INTELMEDIASDKROOT", ""))').stdout().strip()#todo check how to better get env var.
endif

if mfx_home != ''
    mfx_libdir = [mfx_home + '/lib/x64']
    mfx_incdir = include_directories(mfx_home + '/include')
    mfx_lib = cc.find_library('mfx', dirs: mfx_libdir)
    legacy_stdio_dep = cc.find_library('legacy_stdio_definitions') #needed for 2016 R2 and VS2015
    d3d11_dep = cc.find_library('d3d11')
    mfx_dep = declare_dependency(include_directories: mfx_incdir, dependencies: [mfx_lib, d3d11_dep, legacy_stdio_dep])
endif

configure_file(input : 'version.h.in',
               output : 'version.h',
               configuration : core_conf)

subdir('gst-libs')
subdir('parsers')
subdir('gst')
