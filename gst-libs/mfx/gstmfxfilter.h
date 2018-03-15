/*
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

#ifndef GST_MFX_FILTER_H
#define GST_MFX_FILTER_H

#include "gstmfxsurface.h"
#include "gstmfxtaskaggregator.h"
#include "video-format.h"

G_BEGIN_DECLS

#define GST_TYPE_MFX_FILTER (gst_mfx_filter_get_type ())
G_DECLARE_FINAL_TYPE (GstMfxFilter, gst_mfx_filter, GST_MFX, FILTER, GstObject)

typedef enum
{
 GST_MFX_FILTER_STATUS_SUCCESS = 0,
 GST_MFX_FILTER_STATUS_ERROR_ALLOCATION_FAILED,
 GST_MFX_FILTER_STATUS_ERROR_OPERATION_FAILED,
 GST_MFX_FILTER_STATUS_ERROR_INVALID_PARAMETER,
 GST_MFX_FILTER_STATUS_ERROR_UNSUPPORTED_OPERATION,
 GST_MFX_FILTER_STATUS_ERROR_MORE_DATA,
 GST_MFX_FILTER_STATUS_ERROR_MORE_SURFACE,
} GstMfxFilterStatus;

/**
 * GstMfxFilterOp:
 * GST_MFX_FILTER_OP_NONE: No operation.
 * GST_MFX_FILTER_OP_PIXEL_CONVERSION: Pixel conversion operation.
 * GST_MFX_FILTER_OP_SCALE: Scaling operation.
 * GST_MFX_FILTER_OP_CROP: Cropping operation.
 * GST_MFX_FILTER_OP_DEINTERLACING: Deinterlacing operation.
 * GST_MFX_FILTER_OP_DENOISE: Denoise operation.
 * GST_MFX_FILTER_OP_DETAIL: Detail operation.
 * GST_MFX_FILTER_OP_FRAMERATE_CONVERSION: Frame rate conversion operation.
 * GST_MFX_FILTER_OP_BRIGHTNESS: Brightness operation.
 * GST_MFX_FILTER_OP_SATURATION: Saturation operation.
 * GST_MFX_FILTER_OP_HUE: Hue operation.
 * GST_MFX_FILTER_OP_CONTRAST: Contrast operation.
 * GST_MFX_FILTER_OP_FIELD_PROCESSING: Field processing operation.
 * GST_MFX_FILTER_OP_IMAGE_STABILIZATION: Image stabilization operation.
 * GST_MFX_FILTER_OP_ROTATION: Rotation operation.
 * GST_MFX_FILTER_OP_MIRRORING: Mirroring operation.
 */
typedef enum
{
  GST_MFX_FILTER_OP_NONE = 0,
  GST_MFX_FILTER_OP_FORMAT,
  GST_MFX_FILTER_OP_SCALE,
  GST_MFX_FILTER_OP_CROP,
  GST_MFX_FILTER_OP_DEINTERLACING,
  GST_MFX_FILTER_OP_DENOISE,
  GST_MFX_FILTER_OP_DETAIL,
  GST_MFX_FILTER_OP_FRAMERATE_CONVERSION,
  GST_MFX_FILTER_OP_BRIGHTNESS,
  GST_MFX_FILTER_OP_SATURATION,
  GST_MFX_FILTER_OP_HUE,
  GST_MFX_FILTER_OP_CONTRAST,
  GST_MFX_FILTER_OP_FIELD_PROCESSING,
  GST_MFX_FILTER_OP_IMAGE_STABILIZATION,
  GST_MFX_FILTER_OP_ROTATION,
  GST_MFX_FILTER_OP_MIRRORING,
  GST_MFX_FILTER_OP_SCALING_MODE,
} GstMfxFilterOp;

/**
 * GstMfxFilterType:
 * GST_MFX_FILTER_NONE: No filter.
 * GST_MFX_FILTER_DEINTERLACING: Deinterlacing filter.
 * GST_MFX_FILTER_DENOISE: Denoise filter.
 * GST_MFX_FILTER_DETAIL: Detail filter.
 * GST_MFX_FILTER_FRAMERATE_CONVERSION: Framerate conversion filter.
 * GST_MFX_FILTER_PROCAMP: ProcAmp filter.
 * GST_MFX_FILTER_FIELD_PROCESSING: Field processing filter.
 * GST_MFX_FILTER_IMAGE_STABILIZATION: Image stabilization filter.
 * GST_MFX_FILTER_ROTATION: Rotation filter.
 * GST_MFX_FILTER_MIRRORING: Mirroring filter.
 */

typedef enum
{
  GST_MFX_FILTER_NONE = 0,
  GST_MFX_FILTER_DEINTERLACING = (1 << GST_MFX_FILTER_OP_DEINTERLACING),
  GST_MFX_FILTER_DENOISE = (1 << GST_MFX_FILTER_OP_DENOISE),
  GST_MFX_FILTER_DETAIL = (1 << GST_MFX_FILTER_OP_DETAIL),
  GST_MFX_FILTER_FRAMERATE_CONVERSION =
      (1 << GST_MFX_FILTER_OP_FRAMERATE_CONVERSION),
  GST_MFX_FILTER_PROCAMP = (1 << GST_MFX_FILTER_OP_BRIGHTNESS),
  GST_MFX_FILTER_FIELD_PROCESSING =
      (1 << GST_MFX_FILTER_OP_FIELD_PROCESSING),
  GST_MFX_FILTER_IMAGE_STABILIZATION =
      (1 << GST_MFX_FILTER_OP_IMAGE_STABILIZATION),
  GST_MFX_FILTER_ROTATION = (1 << GST_MFX_FILTER_OP_ROTATION),
  GST_MFX_FILTER_MIRRORING = (1 << GST_MFX_FILTER_OP_MIRRORING),
  GST_MFX_FILTER_SCALING_MODE = (1 << GST_MFX_FILTER_OP_SCALING_MODE),
} GstMfxFilterType;

typedef enum
{
  GST_MFX_DEINTERLACE_METHOD_BOB = MFX_DEINTERLACING_BOB,
  GST_MFX_DEINTERLACE_METHOD_ADVANCED = MFX_DEINTERLACING_ADVANCED,
  GST_MFX_DEINTERLACE_METHOD_ADVANCED_NOREF =
      MFX_DEINTERLACING_ADVANCED_NOREF,
#if MSDK_CHECK_VERSION(1,19)
  GST_MFX_DEINTERLACE_METHOD_ADVANCED_SCD = MFX_DEINTERLACING_ADVANCED_SCD,
  GST_MFX_DEINTERLACE_METHOD_FIELD_WEAVING =
      MFX_DEINTERLACING_FIELD_WEAVING,
#endif
} GstMfxDeinterlaceMethod;

typedef enum
{
  GST_MFX_FRC_NONE,
  GST_MFX_FRC_PRESERVE_TIMESTAMP = MFX_FRCALGM_PRESERVE_TIMESTAMP,
  GST_MFX_FRC_DISTRIBUTED_TIMESTAMP = MFX_FRCALGM_DISTRIBUTED_TIMESTAMP,
  GST_MFX_FRC_FRAME_INTERPOLATION = MFX_FRCALGM_FRAME_INTERPOLATION,
  GST_MFX_FRC_FI_PRESERVE_TIMESTAMP =
      MFX_FRCALGM_PRESERVE_TIMESTAMP | MFX_FRCALGM_FRAME_INTERPOLATION,
  GST_MFX_FRC_FI_DISTRIBUTED_TIMESTAMP =
      MFX_FRCALGM_DISTRIBUTED_TIMESTAMP | MFX_FRCALGM_FRAME_INTERPOLATION,
} GstMfxFrcAlgorithm;

/**
 * GstMfxRotation:
 * @GST_MFX_ROTATION_0: the output surface is not rotated.
 * @GST_MFX_ROTATION_90: the output surface is rotated by 90°, clockwise.
 * @GST_MFX_ROTATION_180: the output surface is rotated by 180°, clockwise.
 * @GST_MFX_ROTATION_270: the output surface is rotated by 270°, clockwise.
 */
typedef enum
{
  GST_MFX_ROTATION_0 = 0,
  GST_MFX_ROTATION_90 = 90,
  GST_MFX_ROTATION_180 = 180,
  GST_MFX_ROTATION_270 = 270,
} GstMfxRotation;

#if MSDK_CHECK_VERSION(1,19)
typedef enum
{
  GST_MFX_MIRRORING_DISABLED = MFX_MIRRORING_DISABLED,
  GST_MFX_MIRRORING_HORIZONTAL = MFX_MIRRORING_HORIZONTAL,
  GST_MFX_MIRRORING_VERTICAL = MFX_MIRRORING_VERTICAL,
} GstMfxMirroring;

typedef enum
{
  GST_MFX_SCALING_DEFAULT = MFX_SCALING_MODE_DEFAULT,
  GST_MFX_SCALING_LOWPOWER = MFX_SCALING_MODE_LOWPOWER,
  GST_MFX_SCALING_QUALITY = MFX_SCALING_MODE_QUALITY,
} GstMfxScalingMode;
#endif

GstMfxFilter *
gst_mfx_filter_new (GstMfxTaskAggregator * aggregator,
    gboolean is_system_in, gboolean is_system_out);

GstMfxFilter *
gst_mfx_filter_new_with_task (GstMfxTaskAggregator *
    aggregator, GstMfxTask * task, GstMfxTaskType type,
    gboolean is_system_in, gboolean is_system_out);

GstMfxFilter *
gst_mfx_filter_ref (GstMfxFilter * filter);

void
gst_mfx_filter_unref (GstMfxFilter * filter);

void
gst_mfx_filter_replace (GstMfxFilter ** old_filter_ptr,
    GstMfxFilter * new_filter);

gboolean
gst_mfx_filter_prepare (GstMfxFilter * filter);

GstMfxFilterStatus
gst_mfx_filter_process (GstMfxFilter * filter, GstMfxSurface * surface,
    GstMfxSurface ** out_surface);

GstMfxFilterStatus
gst_mfx_filter_reset (GstMfxFilter * filter);

/* Setters */
void
gst_mfx_filter_set_frame_info (GstMfxFilter * filter,
    mfxFrameInfo * info);

void
gst_mfx_filter_set_frame_info_from_gst_video_info (GstMfxFilter *
    filter, const GstVideoInfo * info);

gboolean
gst_mfx_filter_set_format (GstMfxFilter * filter, mfxU32 fourcc);

gboolean
gst_mfx_filter_set_size (GstMfxFilter * filter, mfxU16 width, mfxU16 height);

gboolean
gst_mfx_filter_set_denoising_level (GstMfxFilter * filter, guint level);

gboolean
gst_mfx_filter_set_detail_level (GstMfxFilter * filter, guint level);

gboolean
gst_mfx_filter_set_hue (GstMfxFilter * filter, gfloat value);

gboolean
gst_mfx_filter_set_saturation (GstMfxFilter * filter, gfloat value);

gboolean
gst_mfx_filter_set_brightness (GstMfxFilter * filter, gfloat value);

gboolean
gst_mfx_filter_set_contrast (GstMfxFilter * filter, gfloat value);

gboolean
gst_mfx_filter_set_rotation (GstMfxFilter * filter, GstMfxRotation angle);

#if MSDK_CHECK_VERSION(1,19)
gboolean
gst_mfx_filter_set_mirroring (GstMfxFilter * filter, GstMfxMirroring mode);

gboolean
gst_mfx_filter_set_scaling_mode (GstMfxFilter * filter, GstMfxScalingMode mode);
#endif // MSDK_CHECK_VERSION

gboolean
gst_mfx_filter_set_deinterlace_method (GstMfxFilter * filter,
    GstMfxDeinterlaceMethod method);

gboolean
gst_mfx_filter_set_framerate (GstMfxFilter * filter,
    guint16 fps_n, guint16 fps_d);

gboolean
gst_mfx_filter_set_frc_algorithm (GstMfxFilter * filter,
    GstMfxFrcAlgorithm alg);

gboolean
gst_mfx_filter_set_async_depth (GstMfxFilter * filter, mfxU16 async_depth);

G_END_DECLS
#endif /* GST_MFX_FILTER_H */
