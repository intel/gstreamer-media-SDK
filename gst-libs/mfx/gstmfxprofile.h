/*
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

#include <mfxjpeg.h>
#include <mfxvp8.h>

G_BEGIN_DECLS

typedef struct _GstMfxProfile GstMfxProfile;
struct _GstMfxProfile
{
  mfxU32 codec;
  mfxU16 profile;
};

const gchar *
gst_mfx_profile_get_name (GstMfxProfile profile);

GstMfxProfile
gst_mfx_profile_from_caps (const GstCaps * caps);

G_END_DECLS
#endif /* GST_MFX_PROFILE_H */
