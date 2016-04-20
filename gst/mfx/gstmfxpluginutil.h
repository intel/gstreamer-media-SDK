#ifndef GST_MFX_PLUGIN_UTIL_H
#define GST_MFX_PLUGIN_UTIL_H

#include "gstmfxtaskaggregator.h"
#include "gstmfxsurfaceproxy.h"
#include "gstmfxvideomemory.h"

gboolean
gst_mfx_ensure_aggregator(GstElement * element);

gboolean
gst_mfx_handle_context_query (GstQuery * query, GstMfxTaskAggregator * context);

gboolean
gst_mfx_append_surface_caps(GstCaps * out_caps, GstCaps * in_caps);

#ifndef G_PRIMITIVE_SWAP
#define G_PRIMITIVE_SWAP(type, a, b) do {       \
	const type t = a; a = b; b = t;         \
} while (0)
#endif

/* Helpers for GValue construction for video formats */
gboolean
gst_mfx_value_set_format(GValue * value, GstVideoFormat format);

/* Helpers to build video caps */
typedef enum
{
	GST_MFX_CAPS_FEATURE_NOT_NEGOTIATED,
	GST_MFX_CAPS_FEATURE_SYSTEM_MEMORY,
	GST_MFX_CAPS_FEATURE_MFX_SURFACE,
} GstMfxCapsFeature;

GstCaps *
gst_mfx_video_format_new_template_caps(GstVideoFormat format);

GstCaps *
gst_mfx_video_format_new_template_caps_from_list(GArray * formats);

GstCaps *
gst_mfx_video_format_new_template_caps_with_features(GstVideoFormat format,
	const gchar * features_string);

GstMfxCapsFeature
gst_mfx_find_preferred_caps_feature(GstPad * pad,
	GstVideoFormat * out_format_ptr);

const gchar *
gst_mfx_caps_feature_to_string(GstMfxCapsFeature feature);

gboolean
gst_mfx_caps_feature_contains(const GstCaps * caps,
	GstMfxCapsFeature feature);

/* Helpers to handle interlaced contents */
# define GST_CAPS_INTERLACED_MODES \
    "interlace-mode = (string){ progressive, interleaved, mixed }"

#define GST_MFX_MAKE_SURFACE_CAPS					\
	GST_VIDEO_CAPS_MAKE_WITH_FEATURES(					\
	GST_CAPS_FEATURE_MEMORY_MFX_SURFACE, "{ NV12, BGRA }")

gboolean
gst_caps_has_mfx_surface(GstCaps * caps);

gboolean
gst_mfx_query_peer_has_raw_caps(GstPad * pad);

void
gst_video_info_change_format(GstVideoInfo * vip, GstVideoFormat format,
    guint width, guint height);

#endif /* GST_MFX_PLUGIN_UTIL_H */
