GStreamer-MSDK
==============
  GStreamer plugins for Intel(R) Media SDK


Overview
--------

GStreamer-MSDK consists of a collection of GStreamer plugins for Intel(R) Media SDK.
This allows users to use Intel(R) Media SDK (MSDK) in their GStreamer-based applications
with minimal knowledge of the MSDK API.

GStreamer-MSDK includes plugins to perform decode, encode, video postprocessing (VPP)
and high performance rendering. Please refer to README.USAGE for more information about these
plugins and their usage.


Features
--------

 - Decode H264 AVC, MPEG-2, JPEG, VC-1, HEVC, VP8 and VP9 videos
 - Compatible with GStreamer-based video players such as Totem, Parole and gst-play
   through playbin element.
 - Support for zero-copy rendering with glimagesink using EGL
 - Support rendering using Wayland renderer
 - Support rendering using X11 renderer with DRI3 backend
 - Support X11 / Wayland rendering using EGL renderer
 - Support VPP acceleration of dynamic procamp control during playback
 - Support for subtitles (text overlay) via MFX VPP subcomposition
 - Support all Media SDK postprocessing capabilities as exposed by the MSDK API
 - Encode / transcode video into H.264, HEVC, MPEG-2 and JPEG formats


Requirements
------------

Software requirements

  * Media Server Studio 2016 Community / Professional Edition (Haswell / Broadwell)
	Media Server Studio 2017 Community / Professional Edition (Broadwell / Skylake)
	MediaSDK 2017 MR1 for Yocto Embedded Edition (Apollo Lake)
  * GStreamer 1.6.x (tested up to GStreamer 1.10.x)
  * gst-plugins-* 1.6.x (tested up to GStreamer 1.10.x)
  * CMake
  
  * Renderers:
      Wayland (>=1.7)
      X11 (DRI 3)
      EGL

Hardware requirements

  * Intel Apollo Lake
  * Intel Haswell / Broadwell /Skylake with Intel HD / Iris Pro graphics

Compiling
---------

GStreamer-MSDK uses the CMake build tool to build the plugins.
Create a build directory within the source directory and run the CMake
command to configure the build.

	mkdir build
	cd build
	cmake ..

To make a debug build:

	cmake .. -DDEBUG=ON
		

To build the plugins for Media Server Studio 2016 Linux Edition:

	cmake .. -DWITH_MSS_2016=ON
		

Only MediaSDK 2017 MR1 For Embedded supports VP9 decode. To disable VP9 decode
for other version of Media SDK:

	cmake .. -DMFX_VP9_DECODER=OFF


Next step is to compile the GStreamer-MSDK plugins:

	make

To install the plugins:

	make install

The plugins will be installed in the /usr/lib/gstreamer-1.0 directory.


Usage
-----

Please refer to README.USAGE for examples on how to accomplish various
video-related tasks with the GStreamer-MSDK plugins.


License
-------

GStreamer Media SDK libraries and plugins are available under the
terms of the GNU Lesser General Public License v2.1+.


Acknowledgements
----------------

This project is heavily based on the well-established GStreamer VAAPI architecture,hence we would
like to publicly thank the GStreamer VAAPI developers for their hard work and contributions.

