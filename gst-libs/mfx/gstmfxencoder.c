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

#include <mfxplugin.h>
#include "gstmfxencoder.h"
#include "gstmfxencoder_priv.h"
#include "gstmfxfilter.h"
#include "gstmfxsurfacepool.h"
#include "gstmfxsurface.h"
#include "gstmfxtask.h"

#define DEBUG 1
#include "gstmfxdebug.h"

#define DEFAULT_ENCODER_PRESET      GST_MFX_ENCODER_PRESET_MEDIUM
#define DEFAULT_QUANTIZER           21
#define DEFAULT_ASYNC_DEPTH         4

G_DEFINE_TYPE_WITH_CODE (GstMfxEncoder, gst_mfx_encoder, GST_TYPE_OBJECT, G_ADD_PRIVATE(GstMfxEncoder));

/* Helper function to create a new encoder property object */
static GstMfxEncoderPropData *
prop_new (gint id, GParamSpec * pspec)
{
  GstMfxEncoderPropData *prop;

  if (!id || !pspec)
    return NULL;

  prop = g_slice_new (GstMfxEncoderPropData);
  if (!prop)
    return NULL;

  prop->prop = id;
  prop->pspec = g_param_spec_ref_sink (pspec);
  return prop;
}

/* Helper function to release a property object and any memory held herein */
static void
prop_free (GstMfxEncoderPropData * prop)
{
  if (!prop)
    return;

  if (prop->pspec) {
    g_param_spec_unref (prop->pspec);
    prop->pspec = NULL;
  }
  g_slice_free (GstMfxEncoderPropData, prop);
}

/* Helper function to lookup the supplied property specification */
static GParamSpec *
prop_find_pspec (GstMfxEncoder * encoder, gint prop_id)
{
  GPtrArray *const props = GST_MFX_ENCODER_GET_PRIVATE(encoder)->properties;
  guint i;

  if (props) {
    for (i = 0; i < props->len; i++) {
      GstMfxEncoderPropInfo *const prop = g_ptr_array_index (props, i);
      if (prop->prop == prop_id)
        return prop->pspec;
    }
  }
  return NULL;
}

/* Create a new array of properties, or NULL on error */
GPtrArray *
gst_mfx_encoder_properties_append (GPtrArray * props, gint prop_id,
    GParamSpec * pspec)
{
  GstMfxEncoderPropData *prop;

  if (!props) {
    props = g_ptr_array_new_with_free_func ((GDestroyNotify) prop_free);
    if (!props)
      return NULL;
  }

  prop = prop_new (prop_id, pspec);
  if (!prop)
    goto error_allocation_failed;
  g_ptr_array_add (props, prop);
  return props;

  /* ERRORS */
error_allocation_failed:
  {
    GST_ERROR ("failed to allocate encoder property info structure");
    g_ptr_array_unref (props);
    return NULL;
  }
}

/* Generate the common set of encoder properties */
GPtrArray *
gst_mfx_encoder_properties_get_default (const GstMfxEncoderClass * klass)
{
  const GstMfxEncoderClassData *const cdata = klass->class_data;
  GPtrArray *props = NULL;

  g_assert (cdata->rate_control_get_type != NULL);

 /**
  * GstMfxEncoder:rate-control
  *
  * The desired rate control mode, expressed as a #GstMfxRateControl.
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_RATECONTROL,
      g_param_spec_enum ("rate-control",
          "Rate control", "Rate control mode",
          cdata->rate_control_get_type (), cdata->default_rate_control,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:bitrate
  *
  * The desired bitrate, expressed in kbps.
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_BITRATE,
      g_param_spec_uint ("bitrate",
          "Bitrate (kbps)",
          "The desired bitrate expressed in kbps (0: auto-calculate)",
          0, G_MAXUINT16, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:brc-multiplier
  *
  * Bit rate control multiplier for high bit rates
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_BRC_MULTIPLIER,
      g_param_spec_uint ("brc-multiplier",
          "Bit rate control multiplier",
          "Multiplier for bit rate control methods to achieve high bit rates",
          0, G_MAXUINT16, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:max-buffer-size
  *
  * Bit rate control multiplier for high bit rates
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_MAX_BUFFER_SIZE,
      g_param_spec_uint ("max-buffer-size",
          "Maximum buffer size (KB)",
          "Maximum possible size of compressed frames",
          0, G_MAXUINT16, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:vbv-max-bitrate
  *
  * Maximum bit rate at which encoded data enters the VBV
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_VBV_MAX_BITRATE,
      g_param_spec_uint ("max-bitrate",
          "Video Buffering Verifier (VBV) maximum bit rate (Kbps)",
          "Maximum bit rate at which encoded data enters the VBV",
          0, G_MAXUINT16, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:idr-interval
  *
  * IDR-frame interval in terms of I-frames.
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_IDR_INTERVAL,
      g_param_spec_int ("idr-interval",
          "IDR interval",
          "Distance (in I-frames) between IDR frames",
          -1, G_MAXINT, -1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:gop-size
  *
  * Number of pictures within the current GOP
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_GOP_SIZE,
      g_param_spec_uint ("gop-size",
          "GOP size",
          "Number of pictures within the current GOP",
          0, G_MAXUINT16, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:gop-distance
  *
  * Distance between I- or P- key frames
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_GOP_REFDIST,
      g_param_spec_int ("gop-distance",
          "GOP reference distance",
          "Distance between I- or P- key frames (1 means no B-frames)",
          -1, 32, -1, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:num-refs
  *
  * Number of reference frames
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_NUM_REFS,
      g_param_spec_uint ("num-refs",
          "Number of reference frames",
          "Number of reference frames",
          0, 16, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:num-slices
  *
  * Number of slices in each video frame
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_NUM_SLICES,
      g_param_spec_uint ("num-slices",
          "Number of slices",
          "Number of slices in each video frame",
          0, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:quantizer
  *
  * Constant quantizer or quality to apply
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_QUANTIZER,
      g_param_spec_uint ("quantizer",
          "Constant quantizer",
          "Constant quantizer or quality to apply", 0, 51,
          DEFAULT_QUANTIZER, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:qpi-offset
  *
  * Quantization parameter offset for I-frames
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_QPI,
      g_param_spec_uint ("qpi-offset",
          "Quantization parameter offset for I-frames",
          "Quantization parameter offset for I-frames", 0, 51,
          0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:qpp-offset
  *
  * Quantization parameter offset for P-frames
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_QPP,
      g_param_spec_uint ("qpp-offset",
          "Quantization parameter offset for P-frames",
          "Quantization parameter offset for P-frames", 0, 51,
          2, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:qpb-offset
  *
  * Quantization parameter offset for B-frames
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_QPB,
      g_param_spec_uint ("qpb-offset",
          "Quantization parameter offset for B-frames",
          "Quantization parameter offset for B-frames", 0, 51,
          4, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:mbbrc
  *
  * MB level bitrate control
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_MBBRC,
      g_param_spec_enum ("mbbrc",
          "MB level bitrate control",
          "MB level bitrate control",
          GST_MFX_TYPE_OPTION, GST_MFX_OPTION_ON,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:extbrc
  *
  * Extended bitrate control (deprecated)
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_EXTBRC,
      g_param_spec_enum ("extbrc",
          "Extended bitrate control",
          "Extended bitrate control (deprecated)",
          GST_MFX_TYPE_OPTION, GST_MFX_OPTION_OFF,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:adaptive-i
  *
  * Adaptive I-frame placement
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_ADAPTIVE_I,
      g_param_spec_enum ("adaptive-i",
          "Adaptive I-frame placement",
          "Adaptive I-frame placement",
          GST_MFX_TYPE_OPTION, GST_MFX_OPTION_OFF,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:adaptive-b
  *
  * Adaptive B-frame placement
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_ADAPTIVE_B,
      g_param_spec_enum ("adaptive-b",
          "Adaptive B-frame placement",
          "Adaptive B-frame placement",
          GST_MFX_TYPE_OPTION, GST_MFX_OPTION_OFF,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:b-pyramid
  *
  * Strategy to choose between I/P/B-frames
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_B_PYRAMID,
      g_param_spec_enum ("b-pyramid",
          "Pyramidal B-frames",
          "Strategy to choose between I/P/B-frames",
          GST_MFX_TYPE_OPTION, GST_MFX_OPTION_AUTO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:accuracy
  *
  * Accuracy of AVBR rate control
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_ACCURACY,
      g_param_spec_uint ("accuracy",
          "AVBR accuracy",
          "Accuracy of AVBR rate control", 0, G_MAXUINT16,
          0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:convergence
  *
  * Convergence of AVBR rate control
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_CONVERGENCE,
      g_param_spec_uint ("convergence",
          "AVBR convergence",
          "Convergence of AVBR rate control", 0, G_MAXUINT16,
          0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:async-depth
  *
  * Number of parallel operations before explicit sync
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_ASYNC_DEPTH,
      g_param_spec_uint ("async-depth",
          "Asynchronous depth",
          "Number of parallel operations before explicit sync", 0, 20,
          DEFAULT_ASYNC_DEPTH, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
  * GstMfxEncoder:preset
  *
  * The desired encoder preset option.
  */
  GST_MFX_ENCODER_PROPERTIES_APPEND (props,
      GST_MFX_ENCODER_PROP_PRESET,
      g_param_spec_enum ("preset",
          "Encoder Preset",
          "Encoder preset option",
          GST_MFX_TYPE_ENCODER_PRESET, DEFAULT_ENCODER_PRESET,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  return props;
}

static void
gst_mfx_encoder_set_frame_info (GstMfxEncoder * encoder)
{
  GstMfxEncoderPrivate *const priv = GST_MFX_ENCODER_GET_PRIVATE(encoder);

  priv->params.mfx.CodecId = priv->codec;

  if (!priv->shared) {
	  priv->params.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    priv->params.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    priv->params.mfx.FrameInfo.PicStruct =
        GST_VIDEO_INFO_IS_INTERLACED (&priv->info) ?
        (GST_VIDEO_INFO_FLAG_IS_SET (&priv->info, GST_VIDEO_FRAME_FLAG_TFF) ?
            MFX_PICSTRUCT_FIELD_TFF : MFX_PICSTRUCT_FIELD_BFF) :
            MFX_PICSTRUCT_PROGRESSIVE;

    priv->params.mfx.FrameInfo.CropX = 0;
    priv->params.mfx.FrameInfo.CropY = 0;
    priv->params.mfx.FrameInfo.CropW = priv->info.width;
    priv->params.mfx.FrameInfo.CropH = priv->info.height;
    priv->params.mfx.FrameInfo.FrameRateExtN = priv->info.fps_n;
    priv->params.mfx.FrameInfo.FrameRateExtD = priv->info.fps_d;
    priv->params.mfx.FrameInfo.AspectRatioW = priv->info.par_n;
    priv->params.mfx.FrameInfo.AspectRatioH = priv->info.par_d;
    priv->params.mfx.FrameInfo.BitDepthChroma = 8;
    priv->params.mfx.FrameInfo.BitDepthLuma = 8;

    if (!g_strcmp0 (priv->plugin_uid, "6fadc791a0c2eb479ab6dcd5ea9da347")) {
		priv->params.mfx.FrameInfo.Width =
          GST_ROUND_UP_32 (priv->info.width);
		priv->params.mfx.FrameInfo.Height =
          GST_ROUND_UP_32 (priv->info.height);
    } else {
		priv->params.mfx.FrameInfo.Width =
          GST_ROUND_UP_16 (priv->info.width);
		priv->params.mfx.FrameInfo.Height =
          (MFX_PICSTRUCT_PROGRESSIVE ==
			  priv->params.mfx.FrameInfo.PicStruct) ?
          GST_ROUND_UP_16 (priv->info.height) :
          GST_ROUND_UP_32 (priv->info.height);
    }

    if (!priv->frame_info.FourCC) {
		priv->frame_info = priv->params.mfx.FrameInfo;
		priv->frame_info.FourCC =
          gst_video_format_to_mfx_fourcc (GST_VIDEO_INFO_FORMAT (&priv->info));
    }
  }
  else {
	  priv->params.mfx.FrameInfo = priv->frame_info;
  }
}

static void
init_encoder_task (GstMfxEncoder * encoder)
{
	GstMfxEncoderPrivate *const priv = GST_MFX_ENCODER_GET_PRIVATE(encoder);

	priv->encode = gst_mfx_task_new (g_object_new(GST_TYPE_MFX_TASK, NULL),
		priv->aggregator, GST_MFX_TASK_ENCODER);
	priv->session = gst_mfx_task_get_session (priv->encode);
  gst_mfx_task_aggregator_set_current_task (priv->aggregator,
	  priv->encode);
}

static gboolean
gst_mfx_encoder_init_properties (GstMfxEncoder * encoder,
    GstMfxTaskAggregator * aggregator, const GstVideoInfo * info,
    gboolean memtype_is_system)
{
  GstMfxEncoderPrivate *const priv = GST_MFX_ENCODER_GET_PRIVATE(encoder);

  priv->aggregator = gst_mfx_task_aggregator_ref (aggregator);

  if ((GST_VIDEO_INFO_FORMAT (info) == GST_VIDEO_FORMAT_NV12) &&
      !memtype_is_system) {
    GstMfxTask *task =
        gst_mfx_task_aggregator_get_current_task (priv->aggregator);

     if (!task) {
       GST_ERROR ("Unable to retrieve upstream MFX task from task aggregator.");
       return FALSE;
     }

    /* This could be a potentially shared task, so get the mfxFrameInfo
     * from the current task to initialize the new encoder
     * or shared VPP / encoder task */
    if (gst_mfx_task_has_type (task, GST_MFX_TASK_DECODER)) {
      mfxVideoParam *params = gst_mfx_task_get_video_params (task);
	  priv->frame_info = params->mfx.FrameInfo;
    }
    else {
      mfxFrameAllocRequest *req = gst_mfx_task_get_request(task);
      if (!req) {
        GST_ERROR ("Unable to retrieve allocation request for encoder task.");
        return FALSE;
      }
	  priv->frame_info = req->Info;
    }

    if (gst_mfx_task_has_video_memory (task) &&
		priv->frame_info.FourCC == MFX_FOURCC_NV12) {
      priv->shared = TRUE;
      priv->encode = task;
      priv->session = gst_mfx_task_get_session (priv->encode);
    }
    else {
      if (!gst_mfx_task_has_video_memory (task))
        memtype_is_system = TRUE;

      /* If this is a peer decoder task, then mark the type as a VPP task
       * that is later on shared with the encoder task. This prevents
       * the decoder module from invoking its own VPP task which cannot
       * be shared by the encoder */
      gst_mfx_task_set_task_type (task,
        gst_mfx_task_get_task_type (task) | GST_MFX_TASK_VPP_IN);

      init_encoder_task (encoder);
      gst_mfx_task_unref (task);
    }
  }
  else {
    init_encoder_task (encoder);
  }
  if (!priv->encode)
    return FALSE;

  priv->bs.MaxLength = info->width * info->height * 4;
  priv->bitstream = g_byte_array_sized_new (priv->bs.MaxLength);
  if (!priv->bitstream)
    return FALSE;
  priv->bs.Data = priv->bitstream->data;
  priv->async_depth = DEFAULT_ASYNC_DEPTH;

  priv->info = *info;
  if (!priv->info.fps_n)
	  priv->info.fps_n = 30;
  priv->duration =
      (priv->info.fps_d / (gdouble)priv->info.fps_n) * 1000000000;
  priv->current_pts = GST_CLOCK_TIME_NONE;

  priv->memtype_is_system = memtype_is_system;

  return TRUE;
}

static void
gst_mfx_encoder_init (GstMfxEncoder * encoder)
{
	GstMfxEncoderPrivate *const priv =
		gst_mfx_encoder_get_instance_private(encoder);

	encoder->priv = priv;
}

/* Base encoder initialization (internal) */
static gboolean
gst_mfx_encoder_create (GstMfxEncoder * encoder,
    GstMfxTaskAggregator * aggregator, const GstVideoInfo * info,
    gboolean memtype_is_system)
{
  GstMfxEncoderClass *const klass = GST_MFX_ENCODER_GET_CLASS (encoder);

  g_return_val_if_fail (aggregator != NULL, FALSE);

#define CHECK_VTABLE_HOOK(FUNC) do {        \
  if (!klass->FUNC)                         \
  goto error_invalid_vtable;                \
  } while (0)

  CHECK_VTABLE_HOOK (create);
  CHECK_VTABLE_HOOK (finalize);
  CHECK_VTABLE_HOOK (get_default_properties);

#undef CHECK_VTABLE_HOOK

  if (!gst_mfx_encoder_init_properties (encoder, aggregator, info,
        memtype_is_system))
    return FALSE;
  if (!klass->create (encoder))
    return FALSE;

  gst_mfx_encoder_set_frame_info (encoder);

  return TRUE;
  /* ERRORS */
error_invalid_vtable:
  {
    GST_ERROR ("invalid subclass hook (internal error)");
    return FALSE;
  }
}

/* Base encoder cleanup (internal) */
void
gst_mfx_encoder_finalize (GObject * object)
{
  GstMfxEncoder * encoder = GST_MFX_ENCODER(object);
  GstMfxEncoderPrivate *const priv = GST_MFX_ENCODER_GET_PRIVATE(encoder);

  GstMfxEncoderClass *const klass = GST_MFX_ENCODER_GET_CLASS (encoder);

  klass->finalize (encoder);

  g_byte_array_unref (priv->bitstream);
  gst_mfx_task_aggregator_unref (priv->aggregator);

  if (priv->properties) {
    g_ptr_array_unref (priv->properties);
	priv->properties = NULL;
  }

  MFXVideoENCODE_Close (priv->session);

  gst_mfx_filter_replace (&priv->filter, NULL);
  gst_mfx_task_replace (&priv->encode, NULL);

}

GstMfxEncoder *
gst_mfx_encoder_new (GstMfxEncoder * encoder, const GstMfxEncoderClass * klass,
    GstMfxTaskAggregator * aggregator, const GstVideoInfo * info,
    gboolean memtype_is_system)
{
  g_return_val_if_fail(encoder != NULL, NULL);
  g_return_val_if_fail(aggregator != NULL, NULL);

  if (!gst_mfx_encoder_create (encoder, aggregator, info, memtype_is_system))
    goto error;

  return encoder;
error:
  gst_object_unref(encoder);
  return NULL;
}

GstMfxEncoder *
gst_mfx_encoder_ref (GstMfxEncoder * encoder)
{
  g_return_val_if_fail (encoder != NULL, NULL);

  return gst_object_ref (GST_OBJECT(encoder));
}

void
gst_mfx_encoder_unref (GstMfxEncoder * encoder)
{
	gst_object_unref(GST_OBJECT (encoder));
}

void
gst_mfx_encoder_replace (GstMfxEncoder ** old_encoder_ptr,
    GstMfxEncoder * new_encoder)
{
  g_return_if_fail (old_encoder_ptr != NULL);

  gst_object_replace((GstObject **) old_encoder_ptr,
      GST_OBJECT (new_encoder));
}

gboolean
gst_mfx_encoder_set_async_depth (GstMfxEncoder * encoder, mfxU16 async_depth)
{
  g_return_val_if_fail (async_depth <= 20, FALSE);

  GST_MFX_ENCODER_GET_PRIVATE(encoder)->async_depth = async_depth;
  return TRUE;
}

gboolean
gst_mfx_encoder_set_gop_refdist (GstMfxEncoder * encoder, gint gop_refdist)
{
  g_return_val_if_fail (gop_refdist <= 32, FALSE);

  GST_MFX_ENCODER_GET_PRIVATE(encoder)->gop_refdist = gop_refdist;
  return TRUE;
}

gboolean
gst_mfx_encoder_set_num_references (GstMfxEncoder * encoder, mfxU16 num_refs)
{
  g_return_val_if_fail (num_refs <= 16, FALSE);

  GST_MFX_ENCODER_GET_PRIVATE(encoder)->num_refs = num_refs;
  return TRUE;
}

gboolean
gst_mfx_encoder_set_quantizer (GstMfxEncoder * encoder, guint quantizer)
{
  g_return_val_if_fail (quantizer <= 51, FALSE);

  GST_MFX_ENCODER_GET_PRIVATE(encoder)->global_quality = quantizer;
  return TRUE;
}

gboolean
gst_mfx_encoder_set_qpi_offset (GstMfxEncoder * encoder, mfxU16 offset)
{
  g_return_val_if_fail (offset <= 51, FALSE);

  GST_MFX_ENCODER_GET_PRIVATE(encoder)->qpi_offset = offset;
  return TRUE;
}

gboolean
gst_mfx_encoder_set_qpp_offset (GstMfxEncoder * encoder, mfxU16 offset)
{
  g_return_val_if_fail (offset <= 51, FALSE);

  GST_MFX_ENCODER_GET_PRIVATE(encoder)->qpp_offset = offset;
  return TRUE;
}

gboolean
gst_mfx_encoder_set_qpb_offset (GstMfxEncoder * encoder, mfxU16 offset)
{
  g_return_val_if_fail (offset <= 51, FALSE);

  GST_MFX_ENCODER_GET_PRIVATE(encoder)->qpb_offset = offset;
  return TRUE;
}


static void
set_default_option_values (GstMfxEncoder * encoder)
{
	GstMfxEncoderPrivate *const priv =
		GST_MFX_ENCODER_GET_PRIVATE(encoder);

  /* Extended coding options, introduced in API 1.0 */
  priv->extco.MECostType = 0;        // reserved, must be 0
  priv->extco.MESearchType = 0;      // reserved, must be 0
  priv->extco.MVSearchWindow.x = 0;  // reserved, must be 0
  priv->extco.MVSearchWindow.y = 0;  // reserved, must be 0
  priv->extco.RefPicListReordering = 0;      // reserved, must be 0
  priv->extco.IntraPredBlockSize = 0;        // reserved, must be 0
  priv->extco.InterPredBlockSize = 0;        // reserved, must be 0
  priv->extco.MVPrecision = 0;       // reserved, must be 0
  priv->extco.EndOfSequence = MFX_CODINGOPTION_UNKNOWN;
  priv->extco.RateDistortionOpt = MFX_CODINGOPTION_UNKNOWN;
  priv->extco.ResetRefList = MFX_CODINGOPTION_UNKNOWN;
  priv->extco.MaxDecFrameBuffering = 0;      // unspecified
  priv->extco.AUDelimiter = MFX_CODINGOPTION_OFF;
  priv->extco.SingleSeiNalUnit = MFX_CODINGOPTION_UNKNOWN;
  priv->extco.PicTimingSEI = MFX_CODINGOPTION_OFF;
  priv->extco.VuiNalHrdParameters = MFX_CODINGOPTION_UNKNOWN;
  priv->extco.FramePicture = MFX_CODINGOPTION_UNKNOWN;
  priv->extco.RefPicMarkRep = MFX_CODINGOPTION_UNKNOWN;
  priv->extco.FieldOutput = MFX_CODINGOPTION_UNKNOWN;
  priv->extco.NalHrdConformance = MFX_CODINGOPTION_UNKNOWN;
  priv->extco.SingleSeiNalUnit = MFX_CODINGOPTION_UNKNOWN;
  priv->extco.VuiVclHrdParameters = MFX_CODINGOPTION_UNKNOWN;
  priv->extco.ViewOutput = MFX_CODINGOPTION_UNKNOWN;
  priv->extco.RecoveryPointSEI = MFX_CODINGOPTION_UNKNOWN;

  /* Extended coding options 2, introduced in API 1.6 */
  priv->extco2.IntRefType = 0;
  priv->extco2.IntRefCycleSize = 2;
  priv->extco2.IntRefQPDelta = 0;
  priv->extco2.MaxFrameSize = 0;
  priv->extco2.BitrateLimit = MFX_CODINGOPTION_ON;
  priv->extco2.MBBRC = MFX_CODINGOPTION_UNKNOWN;
  priv->extco2.ExtBRC = MFX_CODINGOPTION_UNKNOWN;
  priv->extco2.RepeatPPS = MFX_CODINGOPTION_ON;
  priv->extco2.BRefType = MFX_B_REF_UNKNOWN;
  priv->extco2.AdaptiveI = MFX_CODINGOPTION_UNKNOWN;
  priv->extco2.AdaptiveB = MFX_CODINGOPTION_UNKNOWN;
  priv->extco2.NumMbPerSlice = 0;
}

static void
set_extended_coding_options (GstMfxEncoder * encoder)
{
  GstMfxEncoderPrivate *const priv =
		GST_MFX_ENCODER_GET_PRIVATE(encoder);

  priv->extco.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
  priv->extco.Header.BufferSz = sizeof (priv->extco);

  priv->extco2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
  priv->extco2.Header.BufferSz = sizeof (priv->extco2);

  set_default_option_values (encoder);

  if (priv->mbbrc != GST_MFX_OPTION_AUTO)
    priv->extco2.MBBRC =
        priv->mbbrc ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
  if (priv->extbrc != GST_MFX_OPTION_AUTO)
    priv->extco2.ExtBRC =
        priv->extbrc ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
  if (priv->adaptive_i != GST_MFX_OPTION_AUTO)
    priv->extco2.AdaptiveI =
        priv->adaptive_i ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
  if (priv->adaptive_b != GST_MFX_OPTION_AUTO)
    priv->extco2.AdaptiveB =
        priv->adaptive_b ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
  if (priv->b_strategy != GST_MFX_OPTION_AUTO)
    priv->extco2.BRefType =
        priv->b_strategy ? MFX_B_REF_PYRAMID : MFX_B_REF_OFF;

  if (MFX_CODEC_AVC == priv->codec) {
    if (priv->max_slice_size >= 0)
      priv->extco2.MaxSliceSize = priv->max_slice_size;
    priv->extco.CAVLC =
        !priv->use_cabac ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
    priv->extco2.Trellis = priv->trellis;
  }

  switch (priv->rc_method) {
    case GST_MFX_RATECONTROL_LA_BRC:
    case GST_MFX_RATECONTROL_LA_ICQ:
    case GST_MFX_RATECONTROL_LA_HRD:
      if (!priv->la_depth)
        priv->la_depth = 40;
      priv->extco2.LookAheadDepth = CLAMP (priv->la_depth, 10, 100);
      priv->extco2.LookAheadDS = priv->look_ahead_downsampling;
      break;
    default:
      break;
  }

  if ((!((priv->info.width & 15) ^ 8)
        || !((priv->info.height & 15) ^ 8))
      && (priv->codec == MFX_CODEC_HEVC)) {
    priv->exthevc.Header.BufferId = MFX_EXTBUFF_HEVC_PARAM;
    priv->exthevc.Header.BufferSz = sizeof (priv->exthevc);
    priv->exthevc.PicWidthInLumaSamples = priv->info.width;
    priv->exthevc.PicHeightInLumaSamples = priv->info.height;

    priv->extparam_internal[priv->params.NumExtParam++] =
        (mfxExtBuffer *) &priv->exthevc;
  }

  priv->extparam_internal[priv->params.NumExtParam++] =
      (mfxExtBuffer *) &priv->extco;
  priv->extparam_internal[priv->params.NumExtParam++] =
      (mfxExtBuffer *) &priv->extco2;

  priv->params.ExtParam = priv->extparam_internal;
}

/* Many of the default settings here are inspired by Handbrake */
static void
gst_mfx_encoder_set_encoding_params (GstMfxEncoder * encoder)
{
  GstMfxEncoderPrivate *const priv =
		GST_MFX_ENCODER_GET_PRIVATE(encoder);

  priv->params.mfx.CodecProfile = priv->profile;
  priv->params.AsyncDepth = priv->async_depth;

  if (priv->codec != MFX_CODEC_JPEG) {
    switch (priv->rc_method) {
      case GST_MFX_RATECONTROL_CQP:
        priv->params.mfx.QPI =
            CLAMP (priv->global_quality + priv->qpi_offset, 0, 51);
        priv->params.mfx.QPP =
            CLAMP (priv->global_quality + priv->qpp_offset, 0, 51);
        priv->params.mfx.QPB =
            CLAMP (priv->global_quality + priv->qpb_offset, 0, 51);

        /* If set to auto, then enable b-pyramid */
        if (GST_MFX_OPTION_AUTO == priv->b_strategy)
          priv->b_strategy = GST_MFX_OPTION_ON;
        if (!priv->gop_size)
          priv->gop_size = 32;
        priv->gop_refdist =
            priv->gop_refdist < 0 ? 4 : priv->gop_refdist;
        break;
      case GST_MFX_RATECONTROL_VCM:
        priv->gop_refdist = 0;
        break;
      case GST_MFX_RATECONTROL_AVBR:
        priv->params.mfx.Convergence = priv->avbr_convergence;
        priv->params.mfx.Accuracy = priv->avbr_accuracy;
        break;
      case GST_MFX_RATECONTROL_ICQ:
      case GST_MFX_RATECONTROL_LA_ICQ:
        priv->params.mfx.ICQQuality = CLAMP (priv->global_quality, 1, 51);
        break;
      default:
        break;
    }

    if (!priv->gop_size) {
      gdouble frame_rate;

      gst_util_fraction_to_double (priv->info.fps_n,
        priv->info.fps_d, &frame_rate);

      priv->gop_size = (guint16)(frame_rate + 0.5);
    }

    priv->params.mfx.TargetUsage = priv->preset;
    priv->params.mfx.RateControlMethod = priv->rc_method;
    if (priv->idr_interval < 0) {
      if (MFX_CODEC_HEVC == priv->codec)
        priv->params.mfx.IdrInterval = 1;
      else
        priv->params.mfx.IdrInterval = 0;
    }
    else {
      priv->params.mfx.IdrInterval = priv->idr_interval;
    }
    priv->params.mfx.NumRefFrame = priv->num_refs;
    priv->params.mfx.GopPicSize = priv->gop_size;
    priv->params.mfx.NumSlice = priv->num_slices;

    if (priv->bitrate)
      priv->params.mfx.TargetKbps = priv->bitrate;
    if (priv->vbv_max_bitrate > priv->bitrate)
      priv->params.mfx.MaxKbps = priv->vbv_max_bitrate;
    priv->params.mfx.BRCParamMultiplier = priv->brc_multiplier;
    priv->params.mfx.BufferSizeInKB = priv->max_buffer_size;
    priv->params.mfx.GopRefDist =
        priv->gop_refdist < 0 ? 3 : priv->gop_refdist;

    set_extended_coding_options (encoder);
  }
  else {
    priv->params.mfx.Interleaved = 1;
    priv->params.mfx.Quality = priv->jpeg_quality;
    priv->params.mfx.RestartInterval = 0;
  }
}

GstMfxEncoderStatus
gst_mfx_encoder_start (GstMfxEncoder *encoder)
{
  mfxStatus sts = MFX_ERR_NONE;
  mfxFrameAllocRequest *request;
  mfxFrameAllocRequest enc_request;
  gboolean memtype_is_system = FALSE;

  GstMfxEncoderPrivate *const priv =
	  GST_MFX_ENCODER_GET_PRIVATE(encoder);

  /* Use input system memory with SW HEVC encoder or when linked directly
   * with SW HEVC decoder decoding HEVC main-10 streams */
  if (!g_strcmp0 (priv->plugin_uid, "2fca99749fdb49aeb121a5b63ef568f7")
      || (MFX_FOURCC_NV12 == priv->frame_info.FourCC
          && priv->memtype_is_system)
      || MFX_FOURCC_P010 == priv->frame_info.FourCC)
    memtype_is_system = TRUE;

  memset (&enc_request, 0, sizeof (mfxFrameAllocRequest));

  gst_mfx_encoder_set_encoding_params (encoder);

  sts = MFXVideoENCODE_Query (priv->session, &priv->params,
          &priv->params);
  if (MFX_WRN_PARTIAL_ACCELERATION == sts) {
    GST_WARNING ("Partial acceleration %d", sts);
    memtype_is_system = TRUE;
  }
  else if (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts) {
    GST_WARNING ("Incompatible video params detected %d", sts);
  }

  if (memtype_is_system) {
    priv->params.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    gst_mfx_task_ensure_memtype_is_system (priv->encode);
  }
  else {
    priv->params.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    gst_mfx_task_use_video_memory (priv->encode);
  }

  sts = MFXVideoENCODE_QueryIOSurf (priv->session, &priv->params,
          &enc_request);
  if (sts < 0) {
    GST_ERROR ("Unable to query encode allocation request %d", sts);
    return GST_MFX_ENCODER_STATUS_ERROR_ALLOCATION_FAILED;
  }

  if (priv->shared) {
    mfxVideoParam *params = gst_mfx_task_get_video_params (priv->encode);

    if (!params) {
      GST_ERROR ("Unable to retrieve task parameters for encoder.");
      return GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER;
    }

    gst_mfx_task_aggregator_update_peer_memtypes (priv->aggregator,
      memtype_is_system);

    request = gst_mfx_task_get_request(priv->encode);

    /* Re-calculate suggested number of allocated frames for shared task if
     * async-depth of shared task doesn't match with the encoder async-depth */
    params->AsyncDepth = priv->params.AsyncDepth;
    if (gst_mfx_task_has_type (priv->encode, GST_MFX_TASK_VPP_OUT)) {
      mfxFrameAllocRequest vpp_request[2];

      MFXVideoVPP_QueryIOSurf (priv->session, params, vpp_request);
      *request = vpp_request[1];
    }
    else if (gst_mfx_task_has_type (priv->encode, GST_MFX_TASK_DECODER)) {
      MFXVideoDECODE_QueryIOSurf (priv->session, params, request);
    }
    request->NumFrameSuggested +=
        (enc_request.NumFrameSuggested - priv->params.AsyncDepth + 1);
    request->NumFrameMin = request->NumFrameSuggested;

    gst_mfx_task_set_task_type (priv->encode, GST_MFX_TASK_ENCODER);
  }
  else {
    request = &enc_request;
    gst_mfx_task_set_request(priv->encode, request);
  }

  if (MFX_FOURCC_NV12 != priv->frame_info.FourCC) {
    priv->filter = gst_mfx_filter_new_with_task (g_object_new(GST_TYPE_MFX_FILTER, NULL),
		priv->aggregator, priv->encode, GST_MFX_TASK_VPP_OUT,
        priv->memtype_is_system, memtype_is_system);

    request->NumFrameSuggested += (1 - priv->params.AsyncDepth);

    gst_mfx_filter_set_request (priv->filter, request,
        GST_MFX_TASK_VPP_OUT);

    gst_mfx_filter_set_frame_info (priv->filter, &priv->frame_info);
    gst_mfx_filter_set_async_depth (priv->filter, priv->async_depth);
    if (priv->frame_info.FourCC != MFX_FOURCC_NV12)
      gst_mfx_filter_set_format (priv->filter, MFX_FOURCC_NV12);

    if (!gst_mfx_filter_prepare (priv->filter))
      return GST_MFX_ENCODER_STATUS_ERROR_OPERATION_FAILED;
  }

  sts = MFXVideoENCODE_Init (priv->session, &priv->params);
  if (sts < 0) {
    GST_ERROR ("Error initializing the MFX video encoder %d", sts);
    return GST_MFX_ENCODER_STATUS_ERROR_OPERATION_FAILED;
  }

  memset (&priv->params, 0, sizeof(mfxVideoParam));
  MFXVideoENCODE_GetVideoParam (priv->session, &priv->params);

  GST_INFO ("Initialized MFX encoder task using input %s memory surfaces",
    memtype_is_system ? "system" : "video");

  return GST_MFX_ENCODER_STATUS_SUCCESS;
}

static void
calculate_new_pts_and_dts (GstMfxEncoder * encoder, GstVideoCodecFrame * frame)
{
  GstMfxEncoderPrivate *const priv =
		GST_MFX_ENCODER_GET_PRIVATE(encoder);

  frame->duration = priv->duration;
  frame->pts = (priv->bs.TimeStamp / (gdouble) 90000) * 1000000000;
  frame->dts = (priv->bs.DecodeTimeStamp / (gdouble) 90000) * 1000000000;
}

GstMfxEncoderStatus
gst_mfx_encoder_encode (GstMfxEncoder * encoder, GstVideoCodecFrame * frame)
{
  GstMfxSurface *surface, *filter_surface;
  GstMfxFilterStatus filter_sts;
  mfxFrameSurface1 *insurf;
  mfxSyncPoint syncp;
  mfxStatus sts = MFX_ERR_NONE;

  GstMfxEncoderPrivate *const priv =
	  GST_MFX_ENCODER_GET_PRIVATE(encoder);

  surface = gst_video_codec_frame_get_user_data (frame);

  if (priv->filter) {
    filter_sts =
        gst_mfx_filter_process (priv->filter, surface, &filter_surface);
    if (GST_MFX_FILTER_STATUS_SUCCESS != filter_sts) {
      GST_ERROR ("MFX pre-processing error during encode.");
      return GST_MFX_ENCODER_STATUS_ERROR_OPERATION_FAILED;
    }
    surface = filter_surface;
  }

  insurf = gst_mfx_surface_get_frame_surface (surface);

  if (!GST_CLOCK_TIME_IS_VALID(priv->current_pts))
    priv->current_pts = priv->duration * priv->params.mfx.NumRefFrame;
  if (GST_CLOCK_TIME_IS_VALID (frame->pts)
      && (frame->pts > priv->current_pts))
    priv->current_pts = frame->pts;

  insurf->Data.TimeStamp =
      gst_util_uint64_scale (priv->current_pts, 90000, GST_SECOND);
  priv->current_pts += priv->duration;

  do {
    sts = MFXVideoENCODE_EncodeFrameAsync (priv->session,
            NULL, insurf, &priv->bs, &syncp);

    if (MFX_WRN_DEVICE_BUSY == sts)
      g_usleep (500);
    else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
      priv->bs.MaxLength += 1024 * 16;
      priv->bitstream = g_byte_array_set_size (priv->bitstream,
          priv->bs.MaxLength);
      priv->bs.Data = priv->bitstream->data;
    }
  } while (MFX_WRN_DEVICE_BUSY == sts || MFX_ERR_NOT_ENOUGH_BUFFER == sts);

  if (MFX_ERR_MORE_BITSTREAM == sts)
    return GST_MFX_ENCODER_STATUS_NO_BUFFER;
  else if (MFX_ERR_MORE_DATA == sts)
    return GST_MFX_ENCODER_STATUS_MORE_DATA;

  if (sts != MFX_ERR_NONE
      && sts != MFX_ERR_MORE_BITSTREAM
      && sts != MFX_WRN_VIDEO_PARAM_CHANGED) {
    GST_ERROR ("Error during MFX encoding.");
    return GST_MFX_ENCODER_STATUS_ERROR_UNKNOWN;
  }

  if (syncp) {
    do {
      sts = MFXVideoCORE_SyncOperation (priv->session, syncp, 1000);
    } while (MFX_WRN_IN_EXECUTION == sts);

    frame->output_buffer =
        gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
          priv->bs.Data, priv->bs.MaxLength,
          priv->bs.DataOffset, priv->bs.DataLength, NULL, NULL);

    calculate_new_pts_and_dts (encoder, frame);

    priv->bs.DataLength = 0;
  }

  if (priv->bs.FrameType & MFX_FRAMETYPE_IDR
      || priv->bs.FrameType & MFX_FRAMETYPE_xIDR)
    GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);
  else
    GST_VIDEO_CODEC_FRAME_UNSET_SYNC_POINT (frame);

  return GST_MFX_ENCODER_STATUS_SUCCESS;
}

GstMfxEncoderStatus
gst_mfx_encoder_flush (GstMfxEncoder * encoder, GstVideoCodecFrame ** frame)
{
  mfxSyncPoint syncp;
  mfxStatus sts = MFX_ERR_NONE;
  GstMfxEncoderPrivate *const priv =
	  GST_MFX_ENCODER_GET_PRIVATE(encoder);

  do {
    sts = MFXVideoENCODE_EncodeFrameAsync (priv->session,
            NULL, NULL, &priv->bs, &syncp);

    if (MFX_WRN_DEVICE_BUSY == sts)
      g_usleep (500);
    else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
      priv->bs.MaxLength += 1024 * 16;
      priv->bitstream = g_byte_array_set_size (priv->bitstream,
          priv->bs.MaxLength);
      priv->bs.Data = priv->bitstream->data;
    }
  } while (MFX_WRN_DEVICE_BUSY == sts || MFX_ERR_NOT_ENOUGH_BUFFER == sts);

  if (MFX_ERR_NONE != sts)
    return GST_MFX_ENCODER_STATUS_ERROR_OPERATION_FAILED;

  if (syncp) {
    do {
      sts = MFXVideoCORE_SyncOperation (priv->session, syncp, 1000);
    } while (MFX_WRN_IN_EXECUTION == sts);

    if (MFX_ERR_NONE != sts)
      return GST_MFX_ENCODER_STATUS_ERROR_OPERATION_FAILED;

    *frame = g_slice_new0 (GstVideoCodecFrame);

    (*frame)->output_buffer =
        gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
          priv->bs.Data, priv->bs.MaxLength,
          priv->bs.DataOffset, priv->bs.DataLength, NULL, NULL);

    calculate_new_pts_and_dts (encoder, *frame);

    priv->bs.DataLength = 0;
  }

  if (priv->bs.FrameType & MFX_FRAMETYPE_IDR
      || priv->bs.FrameType & MFX_FRAMETYPE_xIDR)
    GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (*frame);
  else
    GST_VIDEO_CODEC_FRAME_UNSET_SYNC_POINT (*frame);

  return GST_MFX_ENCODER_STATUS_SUCCESS;
}

/**
 * gst_mfx_encoder_set_property:
 * @encoder: a #GstMfxEncoder
 * @prop_id: the id of the property to change
 * @value: the new value to set
 *
 * Update the requested property, designed by @prop_id, with the
 * supplied @value. A @NULL value argument resets the property to its
 * default value.
 *
 * Return value: a #GstMfxEncoderStatus
 */
static GstMfxEncoderStatus
set_property (GstMfxEncoder * encoder, gint prop_id, const GValue * value)
{
  GstMfxEncoderStatus status = GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER;
  gboolean success = TRUE;
  GstMfxEncoderPrivate *const priv =
	  GST_MFX_ENCODER_GET_PRIVATE(encoder);
  g_assert (value != NULL);

  /* Handle codec-specific properties */
  if (prop_id < 0) {
    GstMfxEncoderClass *const klass = GST_MFX_ENCODER_GET_CLASS (encoder);

    if (klass->set_property) {
      status = klass->set_property (encoder, prop_id, value);
    }
    return status;
  }

  /* Handle common properties */
  switch (prop_id) {
    case GST_MFX_ENCODER_PROP_RATECONTROL:
      priv->rc_method = g_value_get_enum (value);
      break;
    case GST_MFX_ENCODER_PROP_BITRATE:
		priv->bitrate = g_value_get_uint (value);
      break;
    case GST_MFX_ENCODER_PROP_MAX_BUFFER_SIZE:
		priv->max_buffer_size = g_value_get_uint (value);
      break;
    case GST_MFX_ENCODER_PROP_VBV_MAX_BITRATE:
		priv->vbv_max_bitrate = g_value_get_uint (value);
      break;
    case GST_MFX_ENCODER_PROP_BRC_MULTIPLIER:
		priv->brc_multiplier = g_value_get_uint (value);
      break;
    case GST_MFX_ENCODER_PROP_IDR_INTERVAL:
		priv->idr_interval = g_value_get_int (value);
      break;
    case GST_MFX_ENCODER_PROP_GOP_SIZE:
		priv->gop_size = g_value_get_uint (value);
      break;
    case GST_MFX_ENCODER_PROP_GOP_REFDIST:
      success = gst_mfx_encoder_set_gop_refdist (encoder,
          g_value_get_int (value));
      break;
    case GST_MFX_ENCODER_PROP_NUM_REFS:
      success = gst_mfx_encoder_set_num_references (encoder,
          g_value_get_uint (value));
      break;
    case GST_MFX_ENCODER_PROP_NUM_SLICES:
		priv->num_slices = g_value_get_uint (value);
      break;
    case GST_MFX_ENCODER_PROP_QUANTIZER:
      success = gst_mfx_encoder_set_quantizer (encoder,
          g_value_get_uint (value));
      break;
    case GST_MFX_ENCODER_PROP_QPI:
      success = gst_mfx_encoder_set_qpi_offset (encoder,
          g_value_get_uint (value));
      break;
    case GST_MFX_ENCODER_PROP_QPP:
      success = gst_mfx_encoder_set_qpp_offset (encoder,
          g_value_get_uint (value));
      break;
    case GST_MFX_ENCODER_PROP_QPB:
      success = gst_mfx_encoder_set_qpb_offset (encoder,
          g_value_get_uint (value));
      break;
    case GST_MFX_ENCODER_PROP_MBBRC:
		priv->mbbrc = g_value_get_enum (value);
      break;
    case GST_MFX_ENCODER_PROP_EXTBRC:
		priv->extbrc = g_value_get_enum (value);
      break;
    case GST_MFX_ENCODER_PROP_ADAPTIVE_I:
		priv->adaptive_i = g_value_get_enum (value);
      break;
    case GST_MFX_ENCODER_PROP_ADAPTIVE_B:
		priv->adaptive_b = g_value_get_enum (value);
      break;
    case GST_MFX_ENCODER_PROP_B_PYRAMID:
		priv->b_strategy = g_value_get_enum (value);
      break;
    case GST_MFX_ENCODER_PROP_ACCURACY:
		priv->avbr_accuracy = g_value_get_uint (value);
      break;
    case GST_MFX_ENCODER_PROP_CONVERGENCE:
		priv->avbr_convergence = g_value_get_uint (value);
      break;
    case GST_MFX_ENCODER_PROP_PRESET:
		priv->preset = g_value_get_enum (value);
      break;
    case GST_MFX_ENCODER_PROP_ASYNC_DEPTH:
      success = gst_mfx_encoder_set_async_depth (encoder,
          g_value_get_uint (value));
      break;
    default:
      success = FALSE;
      break;
  }
  return success ? GST_MFX_ENCODER_STATUS_SUCCESS :
      GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER;
}

GstMfxEncoderStatus
gst_mfx_encoder_set_property (GstMfxEncoder * encoder, gint prop_id,
    const GValue * value)
{
  GstMfxEncoderStatus status = GST_MFX_ENCODER_STATUS_SUCCESS;
  GValue default_value = G_VALUE_INIT;

  g_return_val_if_fail (encoder != NULL,
      GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER);

  if (!value) {
    GParamSpec *const pspec = prop_find_pspec (encoder, prop_id);
    if (!pspec)
      goto error_invalid_property;

    g_value_init (&default_value, pspec->value_type);
    g_param_value_set_default (pspec, &default_value);
    value = &default_value;
  }

  status = set_property (encoder, prop_id, value);

  if (default_value.g_type)
    g_value_unset (&default_value);
  return status;
  /* ERRORS */
error_invalid_property:
  {
    GST_ERROR ("unsupported property (%d)", prop_id);
    return GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER;
  }
}

/* Checks video info */
static GstMfxEncoderStatus
check_video_info (GstMfxEncoder * encoder, const GstVideoInfo * vip)
{
  if (!vip->width || !vip->height)
    goto error_invalid_resolution;
  if (vip->fps_n < 0 || vip->fps_d <= 0)
    goto error_invalid_framerate;
  return GST_MFX_ENCODER_STATUS_SUCCESS;
  /* ERRORS */
error_invalid_resolution:
  {
    GST_ERROR ("invalid resolution (%dx%d)", vip->width, vip->height);
    return GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER;
  }
error_invalid_framerate:
  {
    GST_ERROR ("invalid framerate (%d/%d)", vip->fps_n, vip->fps_d);
    return GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER;
  }
}

/**
 * gst_mfx_encoder_set_codec_state:
 * @encoder: a #GstMfxEncoder
 * @state : a #GstVideoCodecState
 *
 * Notifies the encoder about the source surface properties. The
 * accepted set of properties is: video resolution, colorimetry,
 * pixel-aspect-ratio and framerate.
 *
 * This function is a synchronization point for codec configuration.
 * This means that, at this point, the encoder is reconfigured to
 * match the new properties and any other change beyond this point has
 * zero effect.
 *
 * Return value: a #GstMfxEncoderStatus
 */
GstMfxEncoderStatus
gst_mfx_encoder_set_codec_state (GstMfxEncoder * encoder,
    GstVideoCodecState * state)
{
  g_return_val_if_fail (encoder != NULL,
      GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER);
  g_return_val_if_fail (state != NULL,
      GST_MFX_ENCODER_STATUS_ERROR_INVALID_PARAMETER);

  GstMfxEncoderClass *const klass = GST_MFX_ENCODER_GET_CLASS (encoder);
  GstMfxEncoderPrivate *const priv =
	  GST_MFX_ENCODER_GET_PRIVATE(encoder);
  GstMfxEncoderStatus status;

  if (!gst_video_info_is_equal (&state->info, &priv->info)) {
    status = check_video_info (encoder, &state->info);
    if (status != GST_MFX_ENCODER_STATUS_SUCCESS)
      return status;
    priv->info = state->info;
  }
  return klass->reconfigure (encoder);
}

GstMfxEncoderStatus
gst_mfx_encoder_get_codec_data (GstMfxEncoder * encoder,
    GstBuffer ** out_codec_data_ptr)
{
  GstMfxEncoderStatus ret = GST_MFX_ENCODER_STATUS_SUCCESS;
  GstMfxEncoderClass *const klass = GST_MFX_ENCODER_GET_CLASS (encoder);

  *out_codec_data_ptr = NULL;
  if (!klass->get_codec_data)
    return GST_MFX_ENCODER_STATUS_SUCCESS;

  ret = klass->get_codec_data (encoder, out_codec_data_ptr);
  return ret;
}

GType
gst_mfx_encoder_preset_get_type (void)
{
  static volatile gsize g_type = 0;

  static const GEnumValue encoder_preset_values[] = {
    {GST_MFX_ENCODER_PRESET_VERY_FAST,
        "Best speed", "veryfast"},
    {GST_MFX_ENCODER_PRESET_FASTER,
        "Faster", "faster"},
    {GST_MFX_ENCODER_PRESET_FAST,
        "Fast", "fast"},
    {GST_MFX_ENCODER_PRESET_MEDIUM,
        "Balanced", "medium"},
    {GST_MFX_ENCODER_PRESET_SLOW,
        "Slow", "slow"},
    {GST_MFX_ENCODER_PRESET_SLOWER,
        "Slower", "slower"},
    {GST_MFX_ENCODER_PRESET_VERY_SLOW,
        "Best quality", "veryslow"},
    {0, NULL, NULL},
  };

  if (g_once_init_enter (&g_type)) {
    GType type =
        g_enum_register_static ("GstMfxEncoderPreset", encoder_preset_values);
    g_once_init_leave (&g_type, type);
  }
  return g_type;
}

GType
gst_mfx_encoder_trellis_get_type (void)
{
  static volatile gsize g_type = 0;

  static const GEnumValue trellis_values[] = {
    {GST_MFX_ENCODER_TRELLIS_OFF,
        "Disable trellis", "off"},
    {GST_MFX_ENCODER_TRELLIS_I,
        "Enable trellis for I-frames", "i"},
    {GST_MFX_ENCODER_TRELLIS_IP,
        "Enable trellis for I/P-frames", "ip"},
    {GST_MFX_ENCODER_TRELLIS_IPB,
        "Enable trellis for I/P/B-frames", "ipb"},
    {0, NULL, NULL},
  };

  if (g_once_init_enter (&g_type)) {
    GType type =
        g_enum_register_static ("GstMfxEncoderTrellis", trellis_values);
    g_once_init_leave (&g_type, type);
  }
  return g_type;
}

GType
gst_mfx_encoder_lookahead_ds_get_type (void)
{
  static volatile gsize g_type = 0;

  static const GEnumValue lookahead_ds_values[] = {
    {GST_MFX_ENCODER_LOOKAHEAD_DS_AUTO,
        "Let encoder decide", "auto"},
    {GST_MFX_ENCODER_LOOKAHEAD_DS_OFF,
        "No downsampling", "off"},
    {GST_MFX_ENCODER_LOOKAHEAD_DS_2X,
        "Downsample 2x", "2x"},
    {GST_MFX_ENCODER_LOOKAHEAD_DS_4X,
        "Downsample 4x", "4x"},
    {0, NULL, NULL},
  };

  if (g_once_init_enter (&g_type)) {
    GType type = g_enum_register_static ("GstMfxEncoderLookAheadDS",
        lookahead_ds_values);
    g_once_init_leave (&g_type, type);
  }
  return g_type;
}

static void
gst_mfx_encoder_class_init(GstMfxEncoderClass * klass)
{
	GObjectClass *const object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = gst_mfx_encoder_finalize;
}