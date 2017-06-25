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

#include "sysdeps.h"

#include "gstmfxprofile.h"

typedef struct _GstMfxProfileMap GstMfxProfileMap;

struct _GstMfxProfileMap
{
  mfxU16 profile;
  mfxU32 codec;
  const char *media_str;
  const gchar *profile_str;
};

struct map
{
  mfxU16 level;
  const gchar *level_str;
};

/* Profiles */
static const GstMfxProfileMap gst_mfx_profiles[] = {
  {MFX_PROFILE_MPEG2_SIMPLE, MFX_CODEC_MPEG2,
      "video/mpeg, mpegversion=2", "simple"},
  {MFX_PROFILE_MPEG2_MAIN , MFX_CODEC_MPEG2,
      "video/mpeg, mpegversion=2", "main"},
  {MFX_PROFILE_MPEG2_HIGH, MFX_CODEC_MPEG2,
      "video/mpeg, mpegversion=2", "high"},
  {MFX_PROFILE_AVC_BASELINE, MFX_CODEC_AVC,
      "video/x-h264", "baseline"},
  {MFX_PROFILE_AVC_CONSTRAINED_BASELINE, MFX_CODEC_AVC,
      "video/x-h264", "constrained-baseline"},
  {MFX_PROFILE_AVC_MAIN, MFX_CODEC_AVC,
      "video/x-h264", "main"},
  {MFX_PROFILE_AVC_HIGH, MFX_CODEC_AVC,
      "video/x-h264", "high"},
  {MFX_PROFILE_VC1_SIMPLE, MFX_CODEC_VC1,
      "video/x-wmv, wmvversion=3", "simple"},
  {MFX_PROFILE_VC1_MAIN, MFX_CODEC_VC1,
      "video/x-wmv, wmvversion=3", "main"},
  {MFX_PROFILE_VC1_ADVANCED, MFX_CODEC_VC1,
      "video/x-wmv, wmvversion=3, format=(string)WVC1", "advanced"},
  {MFX_PROFILE_JPEG_BASELINE, MFX_CODEC_JPEG,
      "image/jpeg", NULL},
  {MFX_PROFILE_VP8_0, MFX_CODEC_VP8,
      "video/x-vp8", NULL},
#ifdef USE_VP9_DECODER
  {MFX_PROFILE_VP9_0, MFX_CODEC_VP9,
      "video/x-vp9", NULL},
#endif
  {MFX_PROFILE_HEVC_MAIN, MFX_CODEC_HEVC,
      "video/x-h265", "main"},
  {MFX_PROFILE_HEVC_MAIN10, MFX_CODEC_HEVC,
      "video/x-h265", "main-10"},
  {MFX_PROFILE_UNKNOWN,}
};

static const struct map gst_mfx_h264_level_map[] = {
  { MFX_LEVEL_AVC_1,    "1"    },
  { MFX_LEVEL_AVC_1b,   "1b"   },
  { MFX_LEVEL_AVC_11,  "1.1"   },
  { MFX_LEVEL_AVC_12,  "1.2"   },
  { MFX_LEVEL_AVC_13,  "1.3"   },
  { MFX_LEVEL_AVC_2,    "2"    },
  { MFX_LEVEL_AVC_21,  "2.1"   },
  { MFX_LEVEL_AVC_22,  "2.2"   },
  { MFX_LEVEL_AVC_3,    "3"    },
  { MFX_LEVEL_AVC_31,  "3.1"   },
  { MFX_LEVEL_AVC_32,  "3.2"   },
  { MFX_LEVEL_AVC_4,    "4"    },
  { MFX_LEVEL_AVC_41,  "4.1"   },
  { MFX_LEVEL_AVC_42,  "4.2"   },
  { MFX_LEVEL_AVC_5,    "5"    },
  { MFX_LEVEL_AVC_51,  "5.1"   },
  { MFX_LEVEL_AVC_52,  "5.2"   },
  { 0, NULL }
};

static const struct map gst_mfx_hevc_level_map[] = {
  { MFX_LEVEL_HEVC_1,    "1"    },
  { MFX_LEVEL_HEVC_2,    "2"    },
  { MFX_LEVEL_HEVC_21,  "2.1"   },
  { MFX_LEVEL_HEVC_3,    "3"    },
  { MFX_LEVEL_HEVC_31,  "3.1"   },
  { MFX_LEVEL_HEVC_4,    "4"    },
  { MFX_LEVEL_HEVC_41,  "4.1"   },
  { MFX_LEVEL_HEVC_5,    "5"    },
  { MFX_LEVEL_HEVC_51,  "5.1"   },
  { MFX_LEVEL_HEVC_52,  "5.2"   },
  { MFX_LEVEL_HEVC_6,    "6"    },
  { MFX_LEVEL_HEVC_61,  "6.1"   },
  { MFX_LEVEL_HEVC_62,  "6.2"   },
  { 0, NULL }
};

static const struct map gst_mfx_mpeg2_level_map[] = {
  { MFX_LEVEL_MPEG2_LOW,          "low"           },
  { MFX_LEVEL_MPEG2_MAIN,         "main"          },
  { MFX_LEVEL_MPEG2_HIGH,         "high-1440"     },
  { MFX_LEVEL_MPEG2_HIGH1440,     "high"          },
  { 0, NULL }
};

static mfxU16
map_lookup_value(const struct map *m, gchar * level_str)
{
  for (; m && m->level; m++)
    if (strcmp (level_str, m->level_str) == 0)
      return m->level;
  return MFX_LEVEL_UNKNOWN;
}

static mfxU16
gst_mfx_profile_get_level(mfxU32 codec, gchar * level_str)
{
  const struct map *m = NULL;
  switch (codec) {
    case MFX_CODEC_AVC:
      m = gst_mfx_h264_level_map;
      break;
    case MFX_CODEC_HEVC:
      m = gst_mfx_hevc_level_map;
      break;
    case MFX_CODEC_MPEG2:
      m = gst_mfx_mpeg2_level_map;
      break;
    default:
      break;
  }
  return map_lookup_value(m, level_str);
}

static const GstMfxProfileMap *
get_profiles_map (GstMfxProfile profile)
{
  const GstMfxProfileMap *m;

  for (m = gst_mfx_profiles; m->profile; m++)
    if ((m->profile == profile.profile) && (m->codec == profile.codec))
      return m;
  return NULL;
}

/**
 * gst_mfx_profile_get_name:
 * @profile: a #GstMfxProfile
 *
 * Returns a string representation for the supplied @profile.
 *
 * Return value: the statically allocated string representation of @profile
 */
const gchar *
gst_mfx_profile_get_name (GstMfxProfile profile)
{
  const GstMfxProfileMap *const m = get_profiles_map (profile);

  return m ? m->profile_str : NULL;
}

/**
 * gst_mfx_profile_get_media_type_name:
 * @profile: a #GstMfxProfile
 *
 * Returns a string representation for the media type of the supplied
 * @profile.
 *
 * Return value: the statically allocated string representation of
 *   @profile media type
 */
const gchar *
gst_mfx_profile_get_media_type_name (GstMfxProfile profile)
{
  const GstMfxProfileMap *const m = get_profiles_map (profile);

  return m ? m->media_str : NULL;
}

static mfxU16
gst_mfx_profile_from_codec_data_h264 (GstBuffer * buffer)
{
  /* MPEG-4 Part 15: Advanced Video Coding (AVC) file format */
  guchar buf[3];

  if (gst_buffer_extract (buffer, 0, buf, sizeof (buf)) != sizeof (buf))
    return 0;

  if (buf[0] != 1)              /* configurationVersion = 1 */
    return 0;

  switch (buf[1]) {             /* AVCProfileIndication */
    case 66:
      return ((buf[2] & 0x40) ?
          MFX_PROFILE_AVC_CONSTRAINED_BASELINE :
          MFX_PROFILE_AVC_BASELINE);
    case 77:
      return MFX_PROFILE_AVC_MAIN;
    case 100:
      return MFX_PROFILE_AVC_HIGH;

  }
  return 0;
}

static mfxU16
gst_mfx_profile_from_codec_data_h265 (GstBuffer * buffer)
{
  /* ISO/IEC 14496-15:  HEVC file format */
  guchar buf[3];

  if (gst_buffer_extract (buffer, 0, buf, sizeof (buf)) != sizeof (buf))
    return 0;

  if (buf[0] != 1)              /* configurationVersion = 1 */
    return 0;

  if (buf[1] & 0xc0)            /* general_profile_space = 0 */
    return 0;

  switch (buf[1] & 0x1f) {      /* HEVCProfileIndication */
    case 1:
      return MFX_PROFILE_HEVC_MAIN;
    case 2:
      return MFX_PROFILE_HEVC_MAIN10;
    case 3:
      return MFX_PROFILE_HEVC_MAINSP;
  }
  return 0;
}

static GstMfxProfile
gst_mfx_profile_from_codec_data (mfxU32 codec, GstBuffer * buffer)
{
  GstMfxProfile profile = { 0 };

  if (!codec || !buffer)
    return profile;

  profile.codec = codec;
  switch (codec) {
    case MFX_CODEC_AVC:
      profile.profile = gst_mfx_profile_from_codec_data_h264 (buffer);
      break;
    case MFX_CODEC_HEVC:
      profile.profile = gst_mfx_profile_from_codec_data_h265 (buffer);
      break;
    default:
      profile.profile = 0;
      break;
  }
  return profile;
}

/**
 * gst_mfx_profile_from_caps:
 * @caps: a #GstCaps
 *
 * Converts @caps into the corresponding #GstMfxProfile. If the
 * profile cannot be represented by #GstMfxProfile, then zero is
 * returned.
 *
 * Return value: the #GstMfxProfile describing the @caps
 */
GstMfxProfile
gst_mfx_profile_from_caps (const GstCaps * caps)
{
  const GstMfxProfileMap *m;
  GstCaps *caps_test;
  GstStructure *structure;
  const gchar *profile_str, *level_str;
  GstMfxProfile profile = { 0 }, best_profile = { 0 };
  GstBuffer *codec_data = NULL;
  const gchar *name;
  gsize namelen;

  if (!caps)
    return profile;

  structure = gst_caps_get_structure (caps, 0);
  if (!structure)
    return profile;

  name = gst_structure_get_name (structure);
  namelen = strlen (name);

  profile_str = gst_structure_get_string (structure, "profile");
  if (!profile_str) {
    const GValue *v_codec_data;
    v_codec_data = gst_structure_get_value (structure, "codec_data");
    if (v_codec_data)
      codec_data = gst_value_get_buffer (v_codec_data);
  }

  for (m = gst_mfx_profiles; !profile.profile && m->profile; m++) {
    if (strncmp (name, m->media_str, namelen) != 0)
      continue;
    caps_test = gst_caps_from_string (m->media_str);
    if (gst_caps_is_always_compatible (caps, caps_test)) {
      best_profile.codec = m->codec;
      best_profile.profile = m->profile;
      if (profile_str && m->profile_str &&
          strcmp (profile_str, m->profile_str) == 0)
        profile = best_profile;
    }
    if (!profile.profile) {
      profile =
          gst_mfx_profile_from_codec_data (m->codec, codec_data);
    }
    gst_caps_unref (caps_test);
  }

  if (!profile.profile)
    profile = best_profile;
  level_str = gst_structure_get_string (structure, "level");
  if (level_str)
    profile.level = gst_mfx_profile_get_level(profile.codec, level_str);

  return profile;
}

/**
 * gst_mfx_profile_get_caps:
 * @profile: a #GstMfxProfile
 *
 * Converts a #GstMfxProfile into the corresponding #GstCaps. If no
 * matching caps were found, %NULL is returned.
 *
 * Return value: the newly allocated #GstCaps, or %NULL if none was found
 */
GstCaps *
gst_mfx_profile_get_caps (GstMfxProfile profile)
{
  const GstMfxProfileMap *m;
  GstCaps *out_caps, *caps;

  out_caps = gst_caps_new_empty ();
  if (!out_caps)
    return NULL;

  for (m = gst_mfx_profiles; m->profile; m++) {
    if ((m->profile != profile.profile) || (m->codec != profile.codec))
      continue;
    caps = gst_caps_from_string (m->media_str);
    if (!caps)
      continue;
    gst_caps_set_simple (caps, "profile", G_TYPE_STRING, m->profile_str, NULL);
    out_caps = gst_caps_merge (out_caps, caps);
  }
  return out_caps;
}
