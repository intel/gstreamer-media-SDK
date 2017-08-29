/*
 *  Copyright (C) 2012-2014 Intel Corporation
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
 *  Copyright (C) 2016 Intel Corporation
 *    Author: Ishmael Visayana Sameen <ishmael.visayana.sameen@intel.com>
 *    Author: Puunithaaraj Gopal <puunithaaraj.gopal@intel.com>
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

#ifndef GST_MFXPOSTPROC_H
#define GST_MFXPOSTPROC_H

#include "gstmfxpluginbase.h"

#include <gst-libs/mfx/gstmfxsurface.h>
#include <gst-libs/mfx/gstmfxsurfacepool.h>
#include <gst-libs/mfx/gstmfxfilter.h>
#include <gst-libs/mfx/gstmfxvalue.h>

G_BEGIN_DECLS
#define GST_TYPE_MFXPOSTPROC \
  (gst_mfxpostproc_get_type ())
#define GST_MFXPOSTPROC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MFXPOSTPROC, GstMfxPostproc))
#define GST_MFXPOSTPROC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MFXPOSTPROC, \
  GstMfxPostprocClass))
#define GST_IS_MFXPOSTPROC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MFXPOSTPROC))
#define GST_IS_MFXPOSTPROC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MFXPOSTPROC))
#define GST_MFXPOSTPROC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_MFXPOSTPROC, \
  GstMfxPostprocClass))
typedef struct _GstMfxPostproc GstMfxPostproc;
typedef struct _GstMfxPostprocClass GstMfxPostprocClass;

/**
* GstMfxDeinterlaceMode:
* @GST_MFX_DEINTERLACE_MODE_AUTO: Auto detect needs for deinterlacing.
* @GST_MFX_DEINTERLACE_MODE_INTERLACED: Force deinterlacing.
* @GST_MFX_DEINTERLACE_MODE_DISABLED: Never perform deinterlacing.
*/
typedef enum
{
  GST_MFX_DEINTERLACE_MODE_AUTO = 0,
  GST_MFX_DEINTERLACE_MODE_FORCED,
  GST_MFX_DEINTERLACE_MODE_DISABLED,
} GstMfxDeinterlaceMode;


/**
* GstMfxPostprocFlags:
* @GST_MFX_POSTPROC_FLAG_FORMAT: Pixel format conversion.
* @GST_MFX_POSTPROC_FLAG_DENOISE: Noise reduction.
* @GST_MFX_POSTPROC_FLAG_DETAIL: Sharpening.
* @GST_MFX_POSTPROC_FLAG_HUE: Change color hue.
* @GST_MFX_POSTPROC_FLAG_SATURATION: Change saturation.
* @GST_MFX_POSTPROC_FLAG_BRIGHTNESS: Change brightness.
* @GST_MFX_POSTPROC_FLAG_CONTRAST: Change contrast.
* @GST_MFX_POSTPROC_FLAG_DEINTERLACE: Deinterlacing.
* @GST_MFX_POSTPROC_FLAG_ROTATION: Rotation.
* @GST_MFX_POSTPROC_FLAG_SIZE: Video scaling.
*
* The set of operations that are to be performed for each frame.
*/
typedef enum
{
  GST_MFX_POSTPROC_FLAG_FORMAT = 1 << GST_MFX_FILTER_OP_FORMAT,
  GST_MFX_POSTPROC_FLAG_DENOISE = 1 << GST_MFX_FILTER_OP_DENOISE,
  GST_MFX_POSTPROC_FLAG_DETAIL = 1 << GST_MFX_FILTER_OP_DETAIL,
  GST_MFX_POSTPROC_FLAG_HUE = 1 << GST_MFX_FILTER_OP_HUE,
  GST_MFX_POSTPROC_FLAG_SATURATION = 1 << GST_MFX_FILTER_OP_SATURATION,
  GST_MFX_POSTPROC_FLAG_BRIGHTNESS = 1 << GST_MFX_FILTER_OP_BRIGHTNESS,
  GST_MFX_POSTPROC_FLAG_CONTRAST = 1 << GST_MFX_FILTER_OP_CONTRAST,
  GST_MFX_POSTPROC_FLAG_DEINTERLACING = 1 << GST_MFX_FILTER_OP_DEINTERLACING,
  GST_MFX_POSTPROC_FLAG_ROTATION = 1 << GST_MFX_FILTER_OP_ROTATION,
  GST_MFX_POSTPROC_FLAG_FRC = 1 << GST_MFX_FILTER_OP_FRAMERATE_CONVERSION,
  /* Additional custom flags */
  GST_MFX_POSTPROC_FLAG_CUSTOM = 1 << 20,
  GST_MFX_POSTPROC_FLAG_SIZE = GST_MFX_POSTPROC_FLAG_CUSTOM,
} GstMfxPostprocFlags;



struct _GstMfxPostproc
{
  /*< private > */
  GstMfxPluginBase parent_instance;

  GstMfxFilter *filter;
  GstVideoFormat format;        /* output video format */
  guint width;
  guint height;
  guint flags;
  guint async_depth;

  GstCaps *allowed_sinkpad_caps;
  GstVideoInfo sinkpad_info;
  GstCaps *allowed_srcpad_caps;
  GstVideoInfo srcpad_info;

  /* Deinterlacing */
  GstMfxDeinterlaceMode deinterlace_mode;
  GstMfxDeinterlaceMethod deinterlace_method;

  /* Basic filter values */
  guint denoise_level;
  guint detail_level;

  /* Color balance filter values */
  GList *channels;

  gfloat hue;
  gfloat saturation;
  gfloat brightness;
  gfloat contrast;
  guint cb_changed;

  /* FRC */
  GstMfxFrcAlgorithm alg;
  guint16 fps_n;
  guint16 fps_d;
  GstClockTime field_duration;

  /* Rotation angle */
  GstMfxRotation angle;

  guint keep_aspect:1;
};

struct _GstMfxPostprocClass
{
  /*< private > */
  GstMfxPluginBaseClass parent_class;
};

GType
gst_mfxpostproc_get_type (void);

G_END_DECLS
#endif /* GST_MFXPOSTPROC_H */
