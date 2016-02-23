#ifndef GST_MFX_VIDEO_CONTEXT_H
#define GST_MFX_VIDEO_CONTEXT_H

#include "gstmfxcontext.h"

#define GST_MFX_CONTEXT_TYPE_NAME "gst.mfx.Context"

void
gst_mfx_video_context_set_context(GstContext * context,
	GstMfxContext * mfx_ctx);

GstContext *
gst_mfx_video_context_new_with_context(GstMfxContext * mfx_ctx,
	gboolean persistent);

gboolean
gst_mfx_video_context_get_context(GstContext * context,
	GstMfxContext ** context_ptr);

gboolean
gst_mfx_video_context_prepare(GstElement * element,
	GstMfxContext ** context_ptr);

void
gst_mfx_video_context_propagate(GstElement * element,
	GstMfxContext * mfx_ctx);

#endif /* GST_MFX_VIDEO_CONTEXT_H */
