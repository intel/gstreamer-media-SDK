/*
 *  gstmfxcompositefilter.c - MFX composite filter abstraction
 *
 *  Copyright (C) 2017 Intel Corporation
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

#include "sysdeps.h"

#include "gstmfxcompositefilter.h"
#include "gstmfxtaskaggregator.h"
#include "gstmfxtask.h"
#include "gstmfxsurface.h"
#include "gstmfxsurface_vaapi.h"
#include "gstmfxsurfacecomposition.h"


struct _GstMfxCompositeFilter
{
  /*< private > */
  GstMfxMiniObject parent_instance;
  GstMfxTaskAggregator *aggregator;
  GstMfxTask *vpp;
  GstMfxSurface *out_surface;
  gboolean inited;

  mfxSession session;
  mfxFrameInfo frame_info;
  mfxVideoParam params;

  mfxExtBuffer *ext_buffer;
  mfxExtVPPComposite composite;
  guint num_rect;
};

static void
gst_mfx_composite_filter_finalize (GstMfxCompositeFilter * filter)
{
  /* Free allocated memory for filters */
  g_slice_free1 ((sizeof (mfxExtBuffer *)), filter->ext_buffer);
  gst_mfx_surface_replace (&filter->out_surface, NULL);
  gst_mfx_task_aggregator_unref (filter->aggregator);

  MFXVideoVPP_Close (filter->session);

  gst_mfx_task_replace(&filter->vpp, NULL);
}

static gboolean
configure_composite_filter (GstMfxCompositeFilter * filter,
  GstMfxSurfaceComposition * composition)
{
  GstMfxSubpicture *subpicture = NULL;
  guint num_rect = 0;

  g_return_val_if_fail (filter != NULL, FALSE);
  g_return_val_if_fail (composition != NULL, FALSE);

  if (!filter->inited) {
    memset (&filter->composite, 0, sizeof (mfxExtVPPComposite));
    filter->composite.Header.BufferId = MFX_EXTBUFF_VPP_COMPOSITE;
    filter->composite.Header.BufferSz = sizeof (mfxExtVPPComposite);
    filter->composite.Y = 0x10;
    filter->composite.U = 0x80;
    filter->composite.V = 0x80;
  }

  num_rect = gst_mfx_surface_composition_get_num_subpictures (composition);

  if (filter->num_rect != num_rect && filter->composite.InputStream) {
    g_slice_free1(((num_rect+1)*sizeof (mfxVPPCompInputStream)),
        filter->composite.InputStream);
  }

  filter->num_rect = num_rect;

  /* Set number of input stream to composed
   * Input Stream = Number of rectangle + number of base surface*/
  filter->composite.NumInputStream = num_rect + 1;

  if(!filter->composite.InputStream) {
    filter->composite.InputStream =
        g_slice_alloc (filter->composite.NumInputStream *
        sizeof (mfxVPPCompInputStream));
    if (!filter->composite.InputStream)
      return FALSE;
  }

  /* Fill the base picture */
  filter->composite.InputStream[0].DstX = filter->frame_info.CropX;
  filter->composite.InputStream[0].DstY = filter->frame_info.CropY;
  filter->composite.InputStream[0].DstW = filter->frame_info.CropW;
  filter->composite.InputStream[0].DstH = filter->frame_info.CropH;

  /* Fill the subpicture info */
  for (guint i=1; i < filter->composite.NumInputStream; i++) {
    subpicture = gst_mfx_surface_composition_get_subpicture (composition, i-1);
    if (!subpicture)
      return FALSE;
    filter->composite.InputStream[i].DstX = subpicture->sub_rect.x;
    filter->composite.InputStream[i].DstY = subpicture->sub_rect.y;
    filter->composite.InputStream[i].DstH = subpicture->sub_rect.height;
    filter->composite.InputStream[i].DstW = subpicture->sub_rect.width;
    filter->composite.InputStream[i].PixelAlphaEnable = 1;
  }

  filter->ext_buffer = (mfxExtBuffer *) &filter->composite;
  filter->params.NumExtParam = 1;
  filter->params.ExtParam = &filter->ext_buffer;

  return TRUE;
}

static gboolean
gst_mfx_composite_filter_reset (GstMfxCompositeFilter * filter,
    GstMfxSurfaceComposition * composition)
{
  mfxStatus sts = MFX_ERR_NONE;

  g_return_val_if_fail (filter != NULL, FALSE);
  g_return_val_if_fail (composition != NULL, FALSE);

  if (!filter->inited)
    return TRUE;

  if (!configure_composite_filter (filter, composition))
      return FALSE;

  sts = MFXVideoVPP_Reset (filter->session, &filter->params);
  if (sts < 0) {
    GST_ERROR ("Error resetting MFX VPP %d", sts);
    return FALSE;
  }
  return TRUE;
}

static gboolean
gst_mfx_composite_filter_init (GstMfxCompositeFilter * filter,
    GstMfxTaskAggregator * aggregator, gboolean memtype_is_system)
{
  if (memtype_is_system)
    filter->params.IOPattern =
        MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
  else
    filter->params.IOPattern =
        MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
  filter->aggregator = gst_mfx_task_aggregator_ref (aggregator);
  filter->inited = FALSE;
  filter->num_rect = 0;

  filter->vpp =
      gst_mfx_task_new (filter->aggregator, GST_MFX_TASK_VPP_OUT);
  if (!filter->vpp)
    return FALSE;

  filter->session = gst_mfx_task_get_session (filter->vpp);

  return TRUE;
}

static inline const GstMfxMiniObjectClass *
gst_mfx_composite_filter_class (void)
{
  static const GstMfxMiniObjectClass GstMfxCompositeFilterClass = {
    sizeof (GstMfxCompositeFilter),
    (GDestroyNotify) gst_mfx_composite_filter_finalize
  };
  return &GstMfxCompositeFilterClass;
}

GstMfxCompositeFilter *
gst_mfx_composite_filter_new (GstMfxTaskAggregator * aggregator,
    gboolean memtype_is_system)
{
  GstMfxCompositeFilter *filter;

  g_return_val_if_fail (aggregator != NULL, NULL);

  filter = (GstMfxCompositeFilter *)
      gst_mfx_mini_object_new0 (gst_mfx_composite_filter_class ());
  if (!filter)
    return NULL;

  if (!gst_mfx_composite_filter_init (filter, aggregator, memtype_is_system))
    goto error;

  return filter;
error:
  gst_mfx_mini_object_unref(GST_MFX_MINI_OBJECT(filter));
  return NULL;
}

GstMfxCompositeFilter *
gst_mfx_composite_filter_ref(GstMfxCompositeFilter * filter)
{
  g_return_val_if_fail(filter != NULL, NULL);

  return (GstMfxCompositeFilter *)
           gst_mfx_mini_object_ref(GST_MFX_MINI_OBJECT(filter));
}

void
gst_mfx_composite_filter_unref(GstMfxCompositeFilter * filter)
{
  gst_mfx_mini_object_unref(GST_MFX_MINI_OBJECT(filter));
}

void
gst_mfx_composite_filter_replace(GstMfxCompositeFilter ** old_filter_ptr,
  GstMfxCompositeFilter * new_filter)
{
  g_return_if_fail(old_filter_ptr != NULL);

  gst_mfx_mini_object_replace((GstMfxMiniObject **)old_filter_ptr,
    GST_MFX_MINI_OBJECT(new_filter));
}

static gboolean
init_params (GstMfxCompositeFilter * filter,
    GstMfxSurfaceComposition * composition)
{
  filter->params.vpp.In = filter->frame_info;
  filter->params.vpp.Out = filter->frame_info;

  if (!configure_composite_filter (filter, composition)) {
    GST_ERROR ("Error initializing composite filter params.");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_mfx_composite_filter_start (GstMfxCompositeFilter * filter,
  GstMfxSurfaceComposition * composition)
{
  GstMfxSurface *base_surface;
  mfxStatus sts = MFX_ERR_NONE;
  GstVideoInfo info;

  gst_video_info_init(&info);

  base_surface = gst_mfx_surface_composition_get_base_surface (composition);
  filter->frame_info = gst_mfx_surface_get_frame_surface (base_surface)->Info;

  if (!init_params (filter, composition)) {
    GST_ERROR ("Error initializing composite filter params.");
    return FALSE;
  }

  gst_video_info_set_format(&info, GST_MFX_SURFACE_FORMAT (base_surface),
    GST_MFX_SURFACE_WIDTH (base_surface), GST_MFX_SURFACE_HEIGHT (base_surface));

  /* Allocate output surface for final composition */
  if (filter->params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) {
    GstMfxDisplay *display =
        gst_mfx_surface_vaapi_get_display (base_surface);
    filter->out_surface = gst_mfx_surface_vaapi_new (display, &info, NULL);
    gst_mfx_display_unref (display);

    gst_mfx_task_use_video_memory (filter->vpp);
  } else {
    filter->out_surface = gst_mfx_surface_new (&info);
  }
  if (!filter->out_surface)
    return FALSE;

  sts = MFXVideoVPP_Init (filter->session, &filter->params);
  if (sts < 0) {
    GST_ERROR ("Error initializing MFX VPP %d", sts);
    return FALSE;
  }
  return TRUE;
}

gboolean
gst_mfx_composite_filter_apply_composition (GstMfxCompositeFilter * filter,
  GstMfxSurfaceComposition * composition, GstMfxSurface ** out_surface)
{
  GstMfxSurface *surface;
  GstMfxSubpicture *subpicture = NULL;
  mfxFrameSurface1 *insurf, *outsurf = NULL;
  mfxSyncPoint syncp;
  mfxStatus sts = MFX_ERR_NONE;
  guint i, num_subpictures;

  num_subpictures =
      gst_mfx_surface_composition_get_num_subpictures (composition);

  /* Reset filter for every frame because subpicture dimension
   * and position will be different for every frame. */
  if (!gst_mfx_composite_filter_reset (filter, composition))
    return FALSE;

  if (!filter->inited) {
    if (!gst_mfx_composite_filter_start (filter, composition))
      return FALSE;
    filter->inited = TRUE;
  }

  /* First composition with base surface */
  surface = gst_mfx_surface_composition_get_base_surface(composition);
  insurf = gst_mfx_surface_get_frame_surface (surface);

  /* Get output surface */
  outsurf = gst_mfx_surface_get_frame_surface (filter->out_surface);
  do {
    sts =
        MFXVideoVPP_RunFrameVPPAsync (filter->session,
          insurf,
          outsurf,
          NULL,
          &syncp);

    if (MFX_WRN_DEVICE_BUSY == sts)
        g_usleep (100);
  } while (MFX_WRN_DEVICE_BUSY == sts);

  if (MFX_ERR_MORE_DATA == sts) {
    for (i = 0; i < num_subpictures; i++) {
      subpicture =
          gst_mfx_surface_composition_get_subpicture (composition, i);
      insurf = gst_mfx_surface_get_frame_surface (subpicture->surface);

      do {
        sts =
            MFXVideoVPP_RunFrameVPPAsync (filter->session,
              insurf,
              outsurf,
              NULL,
              &syncp);

        if (MFX_WRN_DEVICE_BUSY == sts)
            g_usleep (500);
      } while (MFX_WRN_DEVICE_BUSY == sts);
    }
  }

  if (MFX_ERR_NONE != sts)
    return FALSE;

  do {
    sts = MFXVideoCORE_SyncOperation (filter->session, syncp, 1000);
  } while (MFX_WRN_IN_EXECUTION == sts);

  *out_surface = filter->out_surface;

  return TRUE;
}
