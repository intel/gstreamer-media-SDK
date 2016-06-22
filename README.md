

GStreamer-MSDK
==============
  MediaSDK plugins for GStreamer


Overview
--------

GStreamer-MSDK consists of a collection of Media SDK-based plugins for
GStreamer.

  * 'mfxdecode' is used to decode H.264 AVC, MPEG-2, HEVC, VC-1 and
    JPEG videos using the underlying hardware capabilities. 

  * 'mfxsink' is used to render the decoded frame to a Wayland / X11
    display.

  * 'mfxvpp' is used to do video processing on RAW videos.


License
-------

gstreamer-mediasdk libraries and plugin elements are available
under the terms of the GNU Lesser General Public License v2.1+


Features
--------

 - Decode H264 AVC, MPEG-2, JPEG, VC-1, HEVC, and VP8 Videos
 - Support for the Wayland and X11(with EGL backend)display server
 - Support for headless decode pipelines.
 - Support Media SDK video processing capabilities:


Requirements
------------

Software requirements

  * MediaSDK 2016 R1 for Yocto Embedded (MediaSDK2016R1forYoctoEmbedded-Alpha3.tar.gz) or
    Media Server Studio 2016 Professional R1 (Haswell / Broadwell)
  * GStreamer 1.4.x (up to including GStreamer 1.6):
  * GStreamer-Plugins-Base 1.4.x (up to including GStreamer 1.6):
  * CMake
  
  * Renderers:
      Wayland (>=1.7)
      X11 (With EGL backend)
      EGL

Hardware requirements

  * Intel Apollo Lake
  * Intel Braswell
  * Intel Haswell / Broadwell

Compiling
---------

GStreamer-MSDK uses the CMake build tool to build the plugins.
Create a build directory within the source directory and run the CMake
command to configure the build.

	mkdir build
	cd build
	cmake ..

To make a debug build, please follow the command below:

	cmake .. -DDEBUG=ON
		

To build the plugin for Media Server Studio, please follow the command below:

	cmake .. -DWITH_MSS=ON
		

Next step is to compile the GStreamer-MSDK plugins:

	make

To install the plugins:

        make install

The plugins will be installed in the /usr/lib/gstreamer-1.0 directory.


Usage
-----

 - Play an H.264 video with a MP4 container

    gst-launch-1.0 filesrc location=/path/to/video.mp4 ! \
          qtdemux ! h264parse ! mfxdecode ! mfxsink
 

 - Play an HEVC video with a Matroska container

	gst-launch-1.0 filesrc location=/path/to/video.mkv ! \
			matroskademux ! h265parse ! mfxdecode ! mfxsink

 - Rotate video by 180°
 
	 gst-launch-1.0 filesrc location=/path/to/video.mp4 ! \
			 qtdemux ! h264parse ! mfxdecode ! mfxvpp rotation=180 ! mfxsink


Known Issues
-----------

  * Decode for VC-1 Advance Profile bitstream is not working.


Work In Progress
----------------

 - Codec parsers


Acknowledgements
----------------

This project is heavily based on the well-established GStreamer VAAPI architecture, hence we would like to publicly thank the GStreamer VAAPI developers for their hard work and contributions.

