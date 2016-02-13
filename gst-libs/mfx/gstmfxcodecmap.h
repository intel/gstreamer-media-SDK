#ifndef __GST_MFX_CODECMAP_H__
#define __GST_MFX_CODECMAP_H__

#include <gst/gst.h>
#include <mfxvideo.h>
#include <mfxjpeg.h>

mfxU32
gst_get_mfx_codec_from_caps(GstCaps * caps);


#endif
