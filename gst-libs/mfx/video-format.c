/*
 *  Copyright (C) 2016 Intel Corporation
 *    Author: Puunithaaraj Gopal <puunithaaraj.gopal@intel.com>
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

#include "video-format.h"

typedef struct _GstMfxFormatMap GstMfxFormatMap;

struct _GstMfxFormatMap
{
  GstVideoFormat format;
  mfxU32 mfx_fourcc;
  DXGI_FORMAT dxgi_format;
};

GstMfxFormatMap format_map[] = {
  {GST_VIDEO_FORMAT_NV12, MFX_FOURCC_NV12, DXGI_FORMAT_NV12 },
  {GST_VIDEO_FORMAT_YUY2, MFX_FOURCC_YUY2, DXGI_FORMAT_YUY2 },
  {GST_VIDEO_FORMAT_YV12, MFX_FOURCC_YV12, DXGI_FORMAT_420_OPAQUE },
  {GST_VIDEO_FORMAT_I420, MFX_FOURCC_YV12, DXGI_FORMAT_420_OPAQUE },
  {GST_VIDEO_FORMAT_BGRA, MFX_FOURCC_RGB4, DXGI_FORMAT_B8G8R8A8_UNORM },
  {GST_VIDEO_FORMAT_BGRx, MFX_FOURCC_RGB4, DXGI_FORMAT_B8G8R8A8_UNORM },
  {GST_VIDEO_FORMAT_ENCODED, MFX_FOURCC_P8, DXGI_FORMAT_P8 },
  {GST_VIDEO_FORMAT_ENCODED, MFX_FOURCC_P8_TEXTURE, DXGI_FORMAT_P8 },
#if GST_CHECK_VERSION(1,9,1)
  {GST_VIDEO_FORMAT_P010_10LE, MFX_FOURCC_P010, DXGI_FORMAT_P010 },
#endif
  {0,}
};

GstVideoFormat
gst_video_format_from_mfx_fourcc (mfxU32 fourcc)
{
  GstMfxFormatMap *m;

  for (m = format_map; m->format; m++) {
    if (fourcc == m->mfx_fourcc)
      return m->format;
  }
  return GST_VIDEO_FORMAT_UNKNOWN;
}

mfxU32
gst_video_format_to_mfx_fourcc (GstVideoFormat format)
{
  GstMfxFormatMap *m;

  for (m = format_map; m->format; m++) {
    if (format == m->format)
      return m->mfx_fourcc;
  }
  return 0;
}

DXGI_FORMAT
gst_mfx_fourcc_to_dxgi_format(mfxU32 fourcc)
{
  GstMfxFormatMap *m;

  for (m = format_map; m->format; m++) {
    if (fourcc == m->mfx_fourcc)
      return m->dxgi_format;
  }
  return DXGI_FORMAT_UNKNOWN;
}

