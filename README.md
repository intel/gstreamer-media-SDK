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
 - Decode H264 AVC, MPEG-2, JPEG, VC-1, HEVC (Main and Main 10), VP8 and VP9 videos
 - Compatible with GStreamer-based video players such as Totem, Parole and gst-play
   through playbin element.
 - Support for zero-copy rendering with glimagesink using EGL
 - Support for Direct3D 11 on Windows with zero-copy and deep color rendering for 10-bit video formats
 - Support rendering using Wayland renderer
 - Support rendering using X11 renderer with DRI3 backend
 - Support VPP acceleration of dynamic procamp control during video playback
 - Support for subtitles (text overlay) via MFX VPP surface composition
 - Support all Media SDK postprocessing capabilities as exposed by the MSDK API
 - Encode / transcode video into H.264, HEVC (Main and Main 10), MPEG-2 and JPEG formats


Requirements
------------

**Software requirements**

  * Intel&reg; Media SDK 2016 R2 / 2017 R1 for Windows or  
    Media Server Studio 2017 Community / Professional Edition (Windows / Linux) or  
    Media SDK 2017 for Yocto Embedded Edition (Apollo Lake) or greater
  * GStreamer 1.8.x (1.10.x minimum for Windows, 1.12.x for correct deinterlacing support)
  * gst-plugins-* 1.8.x (tested up to GStreamer 1.12.x, 1.10.x minimum for Windows)
  * Microsoft Visual Studio 2013 / 2015 / 2017 (Windows)
  * Python 3
  * pkg-config
 
  * Renderers:  
    Wayland (>=1.7)  
    X11 (DRI 3)  
    DirectX 11 (Windows 8 / 8.1 / 10)

**Hardware requirements**

  * Intel IvyBridge / Haswell / Broadwell / Skylake / Kabylake with Intel HD / Iris / Iris Pro / Iris Plus graphics
  * Intel Baytrail / Cherrytrail / Apollo Lake


Compiling
---------
GST-MFX uses the Meson and Ninja build tools for building and installation.
You can install the latest meson and ninja packages using the pip installer program generally bundled with recent Python 3 installer packages.
This can be done via the following command (sudo privileges may be required on Linux systems):

	pip3 install --upgrade meson ninja

On Windows, you will need to setup pkg-config and add to PATH.
You can get pkg-config from here (https://sourceforge.net/projects/pkgconfiglite/files/).
Then open VS x64 native tools command prompt, and add gstreamer pkgconfig path to PKG_CONFIG_PATH if not already done:

	set PKG_CONFIG_PATH=%GSTREAMER_1_0_ROOT_X86_64%lib\pkgconfig

Run the Meson command to configure the out-of-source build.

	meson ../gst-mfx-build

To setup a release build:

	meson ../gst-mfx-build_release --buildtype=release
	
To setup a VS2015 project:

	meson ../gst-mfx-build_msvc --backend=vs2015
		
Newer platforms such as Skylake and Kabylake have added video codec support such as HEVC decode / encode and VP9 decode,
but are disabled by default to maximize compatibility with older systems such as Baytrail and Haswell.
To enable these features such as HEVC encode support, from your build dir:

	mesonconf -DMFX_H265_ENCODER=true

For a list of more options when configuring the build, refer to the meson_options.txt file inside the source directory or run mesonconf inside the build directory.

Next step is to build and install the GST-MFX plugins:

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


TODO
----
 - Direct3D 11 - OpenGL interop support
 - HEVC 10-bit encode support on compatible devices


License
-------
GST-MFX libraries and plugins are available under the
terms of the GNU Lesser General Public License v2.1+.


Acknowledgements
----------------
This project is heavily based on the well-established GStreamer VAAPI architecture, hence we would
like to publicly thank the GStreamer VAAPI developers for their hard work and contributions.

