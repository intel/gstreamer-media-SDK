/*
 *  Copyright (C) 2012-2013 Intel Corporation
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
 *  Copyright (C) 2016 Intel Corporation
 *    Author: Ishmael Visayana Sameen <ishmael.visayana.sameen@intel.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

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
#define GST_MFX_MAKE_PROFILE(codec, profile) \
  (MFX_CODEC_##codec ^ MFX_PROFILE_##codec##_##profile)

/**
 * GstMfxProfile:
 * @GST_MFX_PROFILE_UNKNOWN:
 *   Unknown profile, used for initializers
 * @GST_MFX_PROFILE_MPEG2_SIMPLE:
 *   MPEG-2 simple profile
 * @GST_MFX_PROFILE_MPEG2_MAIN:
 *   MPEG-2 main profile
 * @GST_MFX_PROFILE_MPEG2_HIGH:
 *   MPEG-2 high profile
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
 * @GST_MFX_PROFILE_H264_HIGH_422:
 *   H.264 (MPEG-4 Part-10) high 4:2:2 profile [A.2.6], or high 4:2:2
 *   intra profile [A.2.9], depending on constraint_set3_flag
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
 *
 * The set of all profiles for #GstMfxProfile.
 */
typedef enum {
  GST_MFX_PROFILE_UNKNOWN = 0,
  GST_MFX_PROFILE_MPEG2_SIMPLE = GST_MFX_MAKE_PROFILE(MPEG2, SIMPLE),
  GST_MFX_PROFILE_MPEG2_MAIN = GST_MFX_MAKE_PROFILE(MPEG2, MAIN),
  GST_MFX_PROFILE_MPEG2_HIGH = GST_MFX_MAKE_PROFILE(MPEG2, HIGH),
  GST_MFX_PROFILE_AVC_BASELINE = GST_MFX_MAKE_PROFILE(AVC, BASELINE),
  GST_MFX_PROFILE_AVC_CONSTRAINED_BASELINE =
      GST_MFX_MAKE_PROFILE(AVC, CONSTRAINED_BASELINE),
  GST_MFX_PROFILE_AVC_MAIN = GST_MFX_MAKE_PROFILE(AVC, MAIN),
  GST_MFX_PROFILE_AVC_EXTENDED = GST_MFX_MAKE_PROFILE(AVC, EXTENDED),
  GST_MFX_PROFILE_AVC_HIGH = GST_MFX_MAKE_PROFILE(AVC, HIGH),
  GST_MFX_PROFILE_AVC_HIGH_422 = GST_MFX_MAKE_PROFILE(AVC, HIGH_422),
  GST_MFX_PROFILE_VC1_SIMPLE = GST_MFX_MAKE_PROFILE(VC1, SIMPLE),
  GST_MFX_PROFILE_VC1_MAIN = GST_MFX_MAKE_PROFILE(VC1, MAIN),
  GST_MFX_PROFILE_VC1_ADVANCED = GST_MFX_MAKE_PROFILE(VC1, ADVANCED),
  GST_MFX_PROFILE_JPEG_BASELINE = GST_MFX_MAKE_PROFILE(JPEG, BASELINE),
  GST_MFX_PROFILE_VP8 = GST_MFX_MAKE_PROFILE(VP8, 0),
  GST_MFX_PROFILE_HEVC_MAIN = GST_MFX_MAKE_PROFILE(HEVC, MAIN),
  GST_MFX_PROFILE_HEVC_MAIN10 = GST_MFX_MAKE_PROFILE(HEVC, MAIN10),
  GST_MFX_PROFILE_HEVC_MAIN_STILL_PICTURE =
      GST_MFX_MAKE_PROFILE(HEVC, MAINSP),
} GstMfxProfile;

const gchar *
gst_mfx_codec_get_name (mfxU32 codec);

GstMfxProfile
gst_mfx_profile_from_caps (const GstCaps *caps);

const gchar *
gst_mfx_profile_get_name (GstMfxProfile profile);

const gchar *
gst_mfx_profile_get_media_type_name (GstMfxProfile profile);

GstCaps *
gst_mfx_profile_get_caps (GstMfxProfile profile);

mfxU32
gst_mfx_profile_get_codec (GstMfxProfile profile);

mfxU32
gst_mfx_profile_get_codec_profile (GstMfxProfile profile);


G_END_DECLS

#endif /* GST_MFX_PROFILE_H */
