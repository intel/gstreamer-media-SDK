GST-MFX
==============
GStreamer plugins for Intel&reg; Media SDK


Overview
--------
GST-MFX consists of a collection of GStreamer plugins for Intel&reg; Media SDK (MSDK).
This allows users to use MSDK in their GStreamer-based applications with minimal knowledge of 
the MSDK API.

GST-MFX includes plugins to perform decode, encode, video postprocessing (VPP)
and high performance rendering. Please refer to README.USAGE for more information about these
plugins and their usage.


Features
--------
 - Decode H264 AVC, MPEG-2, JPEG, VC-1, HEVC, VP8 and VP9 videos
 - Compatible with GStreamer-based video players such as Totem, Parole and gst-play
   through playbin element.
 - Support for zero-copy rendering with glimagesink using EGL
 - Support Direct3D 11 zero-copy rendering on Windows
 - Support rendering using Wayland renderer
 - Support rendering using X11 renderer with DRI3 backend
 - Support VPP acceleration of dynamic procamp control during video playback
 - Support for subtitles (text overlay) via MFX VPP surface composition
 - Support all Media SDK postprocessing capabilities as exposed by the MSDK API
 - Encode / transcode video into H.264, HEVC, MPEG-2 and JPEG formats


Requirements
------------

**Software requirements**

  * Intel&reg; Media SDK 2016 R2 / 2017 R1 for Windows  
    Media Server Studio 2017 R2 Community / Professional Edition (Windows / Linux)  
    Media SDK 2017 for Yocto Embedded Edition (Apollo Lake)
  * GStreamer 1.6.x (tested up to GStreamer 1.10.x)
  * gst-plugins-* 1.6.x (tested up to GStreamer 1.10.x)
  * CMake
  * pkg-config
 
  * Renderers:  
    Wayland (>=1.7)  
    X11 (DRI 3)  
    Direct3D 11 (Windows 8 / 8.1 / 10)

**Hardware requirements**

  * Intel IvyBridge / Haswell / Broadwell / Skylake / Kabylake with Intel HD / Iris Pro graphics
  * Intel Baytrail / Cherrytrail / Apollo Lake


Compiling
---------
GST-MFX uses the Meson and Ninja build tools for building and installation.
You can install the latest meson and ninja packages using the pip installer program generally bundled with recent Python 3 installer packages.
In Linux for example, you can easily setup both meson and ninja in your system via the following command:

	sudo python3 /usr/bin/pip install meson ninja

On Windows, open VS x64 native tools command prompt, and add gstreamer pkgconfig path to PKG_CONFIG_PATH if not already done:

	set PKG_CONFIG_PATH=%GSTREAMER_1_0_ROOT_X86_64%lib\pkgconfig

Run the Meson command to configure the out-of-source build.

	meson ../gst-mfx-build 

To setup a release build:

	meson ../gst-mfx-build_release --buildtype=release
	
To setup a VS2015 project:

	meson ../gst-mfx-build_msvc --backend=vs2015
		
Only Media SDK 2017 Embedded Edition supports VP9 decode for now. To enable VP9 decode support, from your build dir:

	mesonconf -DUSE_VP9_DECODER=true

For a list of more options when configuring the build, refer to the meson_options.txt file inside the source directory or run mesonconf inside the build directory.

Next step is to compile and install the GST-MFX plugins:

	cd ../gst-mfx-build
	ninja
	sudo ninja install

To uninstall the plugins:

	sudo ninja uninstall

If you intend to rebuild the plugins after making changes to the source code or you would
want to change some of the build options after uninstalling the plugins, it is highly recommended to
simply delete the build folder that you have created and repeat the build process as above.


Usage
-----
Please refer to README.USAGE for examples on how to accomplish various
video-related tasks with the GST-MFX plugins.


License
-------
GST-MFX libraries and plugins are available under the
terms of the GNU Lesser General Public License v2.1+.


Acknowledgements
----------------
This project is heavily based on the well-established GStreamer VAAPI architecture, hence we would
like to publicly thank the GStreamer VAAPI developers for their hard work and contributions.

