#ifndef GST_MFX_VIDEO_CONTEXT_H
#define GST_MFX_VIDEO_CONTEXT_H

#include "gstmfxtaskaggregator.h"

#define GST_MFX_AGGREGATOR_CONTEXT_TYPE_NAME "gst.mfx.Aggregator"

void
gst_mfx_video_context_set_aggregator(GstContext * context,
	GstMfxTaskAggregator * aggregator);

GstContext *
gst_mfx_video_context_new_with_aggregator(GstMfxTaskAggregator * aggregator,
	gboolean persistent);

gboolean
gst_mfx_video_context_get_aggregator(GstContext * context,
	GstMfxTaskAggregator ** aggregator_ptr);

gboolean
gst_mfx_video_context_prepare(GstElement * element,
	GstMfxTaskAggregator ** aggregator_ptr);

void
gst_mfx_video_context_propagate(GstElement * element,
	GstMfxTaskAggregator * aggregator);

#endif /* GST_MFX_VIDEO_CONTEXT_H */
