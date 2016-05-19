#ifndef GST_MFX_PROFILE_H
#define GST_MFX_PROFILE_H

#include <gst/gstvalue.h>
#include <mfxvideo.h>
#include <mfxjpeg.h>
#include <mfxvp8.h>

G_BEGIN_DECLS

/**
* GST_MFX_MAKE_PROFILE:
* @codec: the #GstMfxCodec without the GST_MFX_CODEC_ prefix
* @sub_id: a non-zero sub-codec id
*
* Macro that evaluates to the profile composed from @codec and
* @sub_id.
*/
#define GST_MFX_MAKE_PROFILE(codec, sub_id) \
	(MFX_CODEC_##codec | GST_MAKE_FOURCC(0,0,0,sub_id))

/**
* GstMfxProfile:
* @GST_MFX_PROFILE_UNKNOWN:
*   Unknown profile, used for initializers
* @GST_MFX_PROFILE_MPEG1:
*   MPEG-1
* @GST_MFX_PROFILE_MPEG2_SIMPLE:
*   MPEG-2 simple profile
* @GST_MFX_PROFILE_MPEG2_MAIN:
*   MPEG-2 main profile
* @GST_MFX_PROFILE_MPEG2_HIGH:
*   MPEG-2 high profile
* @GST_MFX_PROFILE_MPEG4_SIMPLE:
*   MPEG-4 Part-2 simple profile
* @GST_MFX_PROFILE_MPEG4_ADVANCED_SIMPLE:
*   MPEG-4 Part-2 advanced simple profile
* @GST_MFX_PROFILE_MPEG4_MAIN:
*   MPEG-4 Part-2 main profile
* @GST_MFX_PROFILE_H263_BASELINE:
*   H.263 baseline profile
* @GST_MFX_PROFILE_H264_BASELINE:
*   H.264 (MPEG-4 Part-10) baseline profile [A.2.1]
* @GST_MFX_PROFILE_H264_CONSTRAINED_BASELINE:
*   H.264 (MPEG-4 Part-10) constrained baseline profile [A.2.1.1]
* @GST_MFX_PROFILE_H264_MAIN:
*   H.264 (MPEG-4 Part-10) main profile [A.2.2]
* @GST_MFX_PROFILE_H264_EXTENDED:
*   H.264 (MPEG-4 Part 10) extended profile [A.2.3]
* @GST_MFX_PROFILE_H264_HIGH:
*   H.264 (MPEG-4 Part-10) high profile [A.2.4]
* @GST_MFX_PROFILE_H264_HIGH10:
*   H.264 (MPEG-4 Part-10) high 10 profile [A.2.5], or high 10 intra
*   profile [A.2.8], depending on constraint_set3_flag
* @GST_MFX_PROFILE_H264_HIGH_422:
*   H.264 (MPEG-4 Part-10) high 4:2:2 profile [A.2.6], or high 4:2:2
*   intra profile [A.2.9], depending on constraint_set3_flag
* @GST_MFX_PROFILE_H264_HIGH_444:
*   H.264 (MPEG-4 Part-10) high 4:4:4 predictive profile [A.2.7], or
*   high 4:4:4 intra profile [A.2.10], depending on constraint_set3_flag
* @GST_MFX_PROFILE_H264_SCALABLE_BASELINE:
*   H.264 (MPEG-4 Part-10) scalable baseline profile [G.10.1.1]
* @GST_MFX_PROFILE_H264_SCALABLE_HIGH:
*   H.264 (MPEG-4 Part-10) scalable high profile [G.10.1.2], or scalable
*   high intra profile [G.10.1.3], depending on constraint_set3_flag
* @GST_MFX_PROFILE_H264_MULTIVIEW_HIGH:
*   H.264 (MPEG-4 Part-10) multiview high profile [H.10.1.1]
* @GST_MFX_PROFILE_H264_STEREO_HIGH:
*   H.264 (MPEG-4 Part-10) stereo high profile [H.10.1.2]
* @GST_MFX_PROFILE_VC1_SIMPLE:
*   VC-1 simple profile
* @GST_MFX_PROFILE_VC1_MAIN:
*   VC-1 main profile
* @GST_MFX_PROFILE_VC1_ADVANCED:
*   VC-1 advanced profile
* @GST_MFX_PROFILE_JPEG_BASELINE:
*   JPEG baseline profile
* @GST_MFX_PROFILE_H265_MAIN:
*   H.265 main profile [A.3.2]
* @GST_MFX_PROFILE_H265_MAIN10:
*   H.265 main 10 profile [A.3.3]
* @GST_MFX_PROFILE_H265_MAIN_STILL_PICTURE:
*   H.265 main still picture profile [A.3.4]
* @GST_MFX_PROFILE_VP9:
*   VP9 prfile 0
*
* The set of all profiles for #GstMfxProfile.
*/
typedef enum {
	GST_MFX_PROFILE_UNKNOWN = 0,
	GST_MFX_PROFILE_MPEG2_SIMPLE = GST_MFX_MAKE_PROFILE(MPEG2, 1),
	GST_MFX_PROFILE_MPEG2_MAIN = GST_MFX_MAKE_PROFILE(MPEG2, 2),
	GST_MFX_PROFILE_MPEG2_HIGH = GST_MFX_MAKE_PROFILE(MPEG2, 3),
	GST_MFX_PROFILE_AVC_BASELINE = GST_MFX_MAKE_PROFILE(AVC, 1),
	GST_MFX_PROFILE_AVC_CONSTRAINED_BASELINE =
	GST_MFX_MAKE_PROFILE(AVC, 9),
	GST_MFX_PROFILE_AVC_MAIN = GST_MFX_MAKE_PROFILE(AVC, 2),
	GST_MFX_PROFILE_AVC_EXTENDED = GST_MFX_MAKE_PROFILE(AVC, 10),
	GST_MFX_PROFILE_AVC_HIGH = GST_MFX_MAKE_PROFILE(AVC, 3),
	GST_MFX_PROFILE_AVC_HIGH10 = GST_MFX_MAKE_PROFILE(AVC, 7),
	GST_MFX_PROFILE_AVC_HIGH_422 = GST_MFX_MAKE_PROFILE(AVC, 4),
	GST_MFX_PROFILE_AVC_MULTIVIEW_HIGH = GST_MFX_MAKE_PROFILE(AVC, 11),
	GST_MFX_PROFILE_AVC_STEREO_HIGH = GST_MFX_MAKE_PROFILE(AVC, 15),
	GST_MFX_PROFILE_VC1_SIMPLE = GST_MFX_MAKE_PROFILE(VC1, 1),
	GST_MFX_PROFILE_VC1_MAIN = GST_MFX_MAKE_PROFILE(VC1, 2),
	GST_MFX_PROFILE_VC1_ADVANCED = GST_MFX_MAKE_PROFILE(VC1, 3),
	GST_MFX_PROFILE_JPEG_BASELINE = GST_MFX_MAKE_PROFILE(JPEG, 1),
	GST_MFX_PROFILE_VP8 = GST_MFX_MAKE_PROFILE(VP8, 1),
	GST_MFX_PROFILE_HEVC_MAIN = GST_MFX_MAKE_PROFILE(HEVC, 1),
	GST_MFX_PROFILE_HEVC_MAIN10 = GST_MFX_MAKE_PROFILE(HEVC, 2),
	GST_MFX_PROFILE_HEVC_MAIN_STILL_PICTURE =
	GST_MFX_MAKE_PROFILE(HEVC, 3),
} GstMfxProfile;

const gchar *
gst_mfx_codec_get_name(mfxU32 codec);

GstMfxProfile
gst_mfx_profile_from_caps(const GstCaps *caps);

const gchar *
gst_mfx_profile_get_name(GstMfxProfile profile);

const gchar *
gst_mfx_profile_get_media_type_name(GstMfxProfile profile);

GstCaps *
gst_mfx_profile_get_caps(GstMfxProfile profile);

mfxU32
gst_mfx_profile_get_codec(GstMfxProfile profile);


G_END_DECLS

#endif /* GST_MFX_PROFILE_H */
