#ifndef GST_MFX_UTILS_H264_H
#define GST_MFX_UTILS_H264_H

#include "gstmfxprofile.h"

G_BEGIN_DECLS

/* Returns a relative score for the supplied GstMfxProfile */
guint
gst_mfx_utils_h264_get_profile_score(GstMfxProfile profile);

/* Returns GstMfxProfile from a string representation */
GstMfxProfile
gst_mfx_utils_h264_get_profile_from_string(const gchar * str);

/* Returns a string representation for the supplied H.264 profile */
const gchar *
gst_mfx_utils_h264_get_profile_string(GstMfxProfile profile);

/* Returns a string representation for the supplied H.264 level */
const gchar *
gst_mfx_utils_h264_get_level_string(mfxU16 level);

G_END_DECLS

#endif /* GST_MFX_UTILS_H264_H */
