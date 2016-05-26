#ifndef GST_MFX_UTILS_H264_H
#define GST_MFX_UTILS_H264_H

#include <va/va.h>
#include "gstmfxprofile.h"
#include "gstmfxsurfaceproxy.h"

G_BEGIN_DECLS

/**
* GstMfxLevelH264:
* @GST_MFX_LEVEL_H264_L1: H.264 level 1.
* @GST_MFX_LEVEL_H264_L1_1: H.264 level 1.1.
* @GST_MFX_LEVEL_H264_L1_2: H.264 level 1.2.
* @GST_MFX_LEVEL_H264_L1_3: H.264 level 1.3.
* @GST_MFX_LEVEL_H264_L2: H.264 level 2.
* @GST_MFX_LEVEL_H264_L2_1: H.264 level 2.1.
* @GST_MFX_LEVEL_H264_L2_2: H.264 level 2.2.
* @GST_MFX_LEVEL_H264_L3: H.264 level 3.
* @GST_MFX_LEVEL_H264_L3_1: H.264 level 3.1.
* @GST_MFX_LEVEL_H264_L3_2: H.264 level 3.2.
* @GST_MFX_LEVEL_H264_L4: H.264 level 4.
* @GST_MFX_LEVEL_H264_L4_1: H.264 level 4.1.
* @GST_MFX_LEVEL_H264_L4_2: H.264 level 4.2.
* @GST_MFX_LEVEL_H264_L5: H.264 level 5.
* @GST_MFX_LEVEL_H264_L5_1: H.264 level 5.1.
* @GST_MFX_LEVEL_H264_L5_2: H.264 level 5.2.
*
* The set of all levels for #GstMfxLevelH264.
*/
typedef enum {
	GST_MFX_LEVEL_H264_L1 = 1,
	GST_MFX_LEVEL_H264_L1b,
	GST_MFX_LEVEL_H264_L1_1,
	GST_MFX_LEVEL_H264_L1_2,
	GST_MFX_LEVEL_H264_L1_3,
	GST_MFX_LEVEL_H264_L2,
	GST_MFX_LEVEL_H264_L2_1,
	GST_MFX_LEVEL_H264_L2_2,
	GST_MFX_LEVEL_H264_L3,
	GST_MFX_LEVEL_H264_L3_1,
	GST_MFX_LEVEL_H264_L3_2,
	GST_MFX_LEVEL_H264_L4,
	GST_MFX_LEVEL_H264_L4_1,
	GST_MFX_LEVEL_H264_L4_2,
	GST_MFX_LEVEL_H264_L5,
	GST_MFX_LEVEL_H264_L5_1,
	GST_MFX_LEVEL_H264_L5_2,
} GstMfxLevelH264;

/* Returns a relative score for the supplied GstMfxProfile */
guint
gst_mfx_utils_h264_get_profile_score(GstMfxProfile profile);

/* Returns GstMfxProfile from a string representation */
GstMfxProfile
gst_mfx_utils_h264_get_profile_from_string(const gchar * str);

/* Returns a string representation for the supplied H.264 profile */
const gchar *
gst_mfx_utils_h264_get_profile_string(GstMfxProfile profile);

/* Returns GstMfxLevelH264 from a string representation */
GstMfxLevelH264
gst_mfx_utils_h264_get_level_from_string(const gchar * str);

/* Returns a string representation for the supplied H.264 level */
const gchar *
gst_mfx_utils_h264_get_level_string(GstMfxLevelH264 level);

G_END_DECLS

#endif /* GST_MFX_UTILS_H264_H */