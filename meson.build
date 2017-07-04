project('gst_mfx', 'c', 'cpp',
  version : '2.0.0',
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
libversion = '@0@.@1@.0'.format(soversion, gst_version_minor.to_int() * 100 + gst_version_micro.to_int())

plugins_install_dir = '@0@/gstreamer-1.0'.format(get_option('libdir'))

cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')

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

core_conf.set('GETTEXT_PACKAGE', 'gst-mfx')
core_conf.set('PACKAGE', 'gst_mfx')
core_conf.set('VERSION', '@0@'.format(gst_version))
core_conf.set('PACKAGE_VERSION', '@0@'.format(gst_version))
core_conf.set('GST_PACKAGE_NAME', 'GStreamer MFX')
core_conf.set('GST_PACKAGE_ORIGIN', 'Unknown package origin')
core_conf.set('GST_API_VERSION', '@0@'.format(api_version))
core_conf.set('GST_INSTALL_PLUGINS_HELPER', '/FIXME')
core_conf.set('GST_DATADIR', '/FIXME')
core_conf.set('GST_LICENSE', 'LGPL')

gst_mfx_args = []
gst_mfx_deps = []

# GStreamer base dependencies
glib_deps = [dependency('glib-2.0'), dependency('gobject-2.0'), dependency('gio-2.0'), dependency('gmodule-2.0')]
gst_mfx_deps += glib_deps

gst_req = '>= 1.6.0'

gst_dep = dependency('gstreamer-1.0', version: gst_req,
  fallback : ['gstreamer', 'gst_dep'])
gstbase_dep = dependency('gstreamer-base-1.0', version : gst_req,
  fallback : ['gstreamer', 'gstbase_dep'])
gstvideo_dep = dependency('gstreamer-video-1.0', version : gst_req,
  fallback : ['gst-plugins-base', 'gstvideo_dep'])
gstallocators_dep = dependency('gstreamer-allocators-1.0', version: gst_req,
  fallback : ['gst-plugins-base', 'gstallocators_dep'])
gstcodecparsers_dep = dependency('gstreamer-codecparsers-1.0', version: gst_req,
  fallback : ['gst-plugins-bad', 'gstcodecparsers_dep'], required: false)
gstpbutils_dep = dependency('gstreamer-pbutils-1.0', version: gst_req,
  fallback : ['gst-plugins-base', 'gstpbutils_dep'], required: false)
  
gst_mfx_deps += [gst_dep, gstvideo_dep, gstallocators_dep]

with_codecparsers = false
if gstcodecparsers_dep.found()
  gst_mfx_deps += [gstcodecparsers_dep]
  with_codecparsers = true
endif

with_pbutils = false
if gstpbutils_dep.found()
  gst_mfx_deps += [gstpbutils_dep]
  with_pbutils = true
endif

mfx_dep = dependency('mfx', required: false) #find libmfx using pkgconfig first
if mfx_dep.found() == false # otherwise look into Media SDK/Media Server Studio directories
  python = find_program('python')
  mfx_home = run_command(python, '-c', 'import os; print(os.environ.get("INTELMEDIASDKROOT", os.environ.get("MFX_HOME", "")))').stdout().strip()
  
  if mfx_home != ''
    mfx_libdir = [mfx_home + '/lib/lin_x64', mfx_home + '/lib/x64']
    mfx_incdir = include_directories(mfx_home + '/include')
    mfx_lib = cxx.find_library('mfx', dirs: mfx_libdir)
    if host_machine.system() == 'windows' # windows Media SDK / Media Server Studio prebuilt lib requires legacy_stdio_definitions
      legacy_stdio_dep = cc.find_library('legacy_stdio_definitions')
      mfx_dep = declare_dependency(include_directories: mfx_incdir, dependencies: [mfx_lib, legacy_stdio_dep])
    else
      mfx_dep = declare_dependency(include_directories: mfx_incdir, dependencies: mfx_lib)
    endif
  endif
endif

gst_mfx_deps += mfx_dep

if host_machine.system() == 'windows'
  gst_mfx_deps += cc.find_library('d3d11')
  gst_mfx_args += ['-DWITH_D3D11_BACKEND']
  gst_mfx_deps += cc.find_library('stdc++', required: false)
else
  driver_deps = [dependency ('libva'), dependency ('libdrm'), dependency ('libdrm_intel'),
    dependency ('libva-drm'), dependency('libudev')]
  gst_mfx_deps += driver_deps
  gst_mfx_deps += cc.find_library('stdc++')
  gst_mfx_deps += cc.find_library('dl')
  gstgl_dep = dependency('gstreamer-gl-1.0', version: gst_req,
    fallback : ['gst-plugins-bad', 'gstgl_dep'], required: false)
  if gstgl_dep.found()
    gst_mfx_deps += [gstgl_dep]
    gst_mfx_args += ['-DHAVE_GST_GL_LIBS']
  endif
  gst_mfx_args += ['-DWITH_LIBVA_BACKEND']
endif

config_inc = include_directories('.')

mfx_decoder = get_option('MFX_DECODER')
if mfx_decoder
  gst_mfx_args += ['-DMFX_DECODER']
endif

if get_option('MFX_VPP')
  gst_mfx_args += ['-DMFX_VPP']
endif

mfx_sink = false
with_wayland = false
with_x11 = false
with_d3d11 = false
if host_machine.system() == 'windows'
  if get_option('USE_D3D11_RENDERER')
    gst_mfx_args += ['-DMFX_SINK']
    with_d3d11 = true
    mfx_sink = true
  endif
else
  if get_option ('USE_WAYLAND_RENDERER')
    gst_mfx_args += ['-DUSE_WAYLAND']
    mfx_sink = true
	
    wayland_client_dep = dependency('wayland-client', required: true)
    if wayland_client_dep.found()
      gst_mfx_deps += wayland_client_dep
	  with_wayland = true
    endif
    wayland_cursor_dep = dependency('wayland-cursor', required: false)
    if wayland_cursor_dep.found()
	  gst_mfx_deps += wayland_cursor_dep
    endif
  endif
  
  if get_option ('USE_DRI3_RENDERER')
    gst_mfx_args += ['-DUSE_DRI3']
    mfx_sink = true
	
    x11_dep = dependency('x11', required: true)
    if x11_dep.found()
      gst_mfx_deps += [x11_dep]
      with_x11 = true
    endif
    x11_optional_deps = [
      ['xcb-dri3', '-DHAVE_XCBDRI3'],
      ['xcb-present', '-DHAVE_XCBPRESENT'],
      ['xkb-common', '-DHAVE_XKBLIB'],
      ['xrandr', '-DHAVE_XRANDR'],
      ['xrender', '-DHAVE_XRENDER'],
      ['x11-xcb', '-DHAVE_X11XCB'],
    ]
    foreach d: x11_optional_deps
      dep = dependency(d.get(0), required: false)
      if dep.found()
        gst_mfx_deps += [dep]
        gst_mfx_args += [d.get(1)]
      endif
    endforeach
  endif
  
  if mfx_sink
    gst_mfx_args += ['-DMFX_SINK']
  endif
endif

mfx_encoder = get_option('MFX_ENCODER')

configure_file(input : 'version.h.in',
               output : 'version.h',
               configuration : core_conf)

subdir('gst-libs')
subdir('parsers')
subdir('gst')
