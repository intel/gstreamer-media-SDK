#ifndef GST_MFX_VIDEO_CONTEXT_H
#define GST_MFX_VIDEO_CONTEXT_H

#include "gstmfxdisplay.h"

#define GST_MFX_DISPLAY_CONTEXT_TYPE_NAME "gst.mfx.Display"

void
gst_mfx_video_context_set_display(GstContext * context,
	GstMfxDisplay * display);

GstContext *
gst_mfx_video_context_new_with_display(GstMfxDisplay * display,
	gboolean persistent);

gboolean
gst_mfx_video_context_get_display(GstContext * context,
	GstMfxDisplay ** display_ptr);

gboolean
gst_mfx_video_context_prepare(GstElement * element,
	GstMfxDisplay ** display_ptr);

void
gst_mfx_video_context_propagate(GstElement * element,
	GstMfxDisplay * display);

#endif /* GST_MFX_VIDEO_CONTEXT_H */