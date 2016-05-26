#ifndef GST_MFX_UTILS_H264_PRIV_H
#define GST_MFX_UTILS_H264_PRIV_H

#include "gstmfxutils_h264.h"

G_BEGIN_DECLS

/**
* GstMfxH264LevelLimits:
* @level: the #GstMfxLevelH264
* @level_idc: the H.264 level_idc value
* @MaxMBPS: the maximum macroblock processing rate (MB/sec)
* @MaxFS: the maximum frame size (MBs)
* @MaxDpbMbs: the maxium decoded picture buffer size (MBs)
* @MaxBR: the maximum video bit rate (kbps)
* @MaxCPB: the maximum CPB size (kbits)
*
* The data structure that describes the limits of an H.264 level.
*/
typedef struct {
	GstMfxLevelH264 level;
	guint8 level_idc;
	guint32 MaxMBPS;
	guint32 MaxFS;
	guint32 MaxDpbMbs;
	guint32 MaxBR;
	guint32 MaxCPB;
} GstMfxH264LevelLimits;

/* Returns GstMfxProfile from H.264 profile_idc value */
GstMfxProfile
gst_mfx_utils_h264_get_profile(guint8 profile_idc);

/* Returns H.264 profile_idc value from GstMfxProfile */
guint8
gst_mfx_utils_h264_get_profile_idc(GstMfxProfile profile);

/* Returns GstMfxLevelH264 from H.264 level_idc value */
GstMfxLevelH264
gst_mfx_utils_h264_get_level(guint8 level_idc);

/* Returns H.264 level_idc value from GstMfxLevelH264 */
guint8
gst_mfx_utils_h264_get_level_idc(GstMfxLevelH264 level);

/* Returns level limits as specified in Table A-1 of the H.264 standard */
const GstMfxH264LevelLimits *
gst_mfx_utils_h264_get_level_limits(GstMfxLevelH264 level);

/* Returns the Table A-1 specification */
const GstMfxH264LevelLimits *
gst_mfx_utils_h264_get_level_limits_table(guint * out_length_ptr);

G_END_DECLS

#endif /* GST_MFX_UTILS_H264_PRIV_H */