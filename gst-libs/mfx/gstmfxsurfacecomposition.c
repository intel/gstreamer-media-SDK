/*
 *  gstmfxsubpicturecomposition.c - MFX subpicture composition abstraction
 *
 *  Copyright (C) 2017 Intel Corporation
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

#include "gstmfxsurfacecomposition.h"
#include "gstmfxsurface.h"
#include "gstmfxsurface_vaapi.h"

#define DEBUG 1
#include "gstmfxdebug.h"

struct _GstMfxSurfaceComposition
{
  /*< private > */
  GstMfxMiniObject parent_instance;

  GPtrArray *subpictures;
  GstMfxSurface *base_surface;
};

static void
destroy_subpicture (GstMfxSubpicture * subpicture)
{
  gst_mfx_surface_unref(subpicture->surface);
  g_slice_free(GstMfxSubpicture, subpicture);
}

static gboolean
create_subpicture (GstMfxSurfaceComposition * composition,
  GstVideoOverlayRectangle * rect)
{
  GstMfxSubpicture *subpicture;
  GstBuffer *buffer;
  GstVideoMeta *vmeta;
  guint8 *data;
  guint stride;
  GstMapInfo map_info;
  GstVideoInfo info;

  gst_video_info_init(&info);

  buffer = gst_video_overlay_rectangle_get_pixels_unscaled_argb (rect,
    gst_video_overlay_rectangle_get_flags(rect));
  if (!buffer)
    return FALSE;

  vmeta = gst_buffer_get_video_meta(buffer);
  if (!vmeta)
    return FALSE;

  gst_video_info_set_format(&info, GST_VIDEO_FORMAT_BGRA,
    vmeta->width, vmeta->height);

  if (!gst_video_meta_map(vmeta, 0, &map_info, (gpointer *)& data,
      (gint *)& stride, GST_MAP_READ))
    return FALSE;

  subpicture = g_slice_new0(GstMfxSubpicture);

  if (gst_mfx_surface_has_video_memory (composition->base_surface)) {
    GstMfxDisplay *display =
        gst_mfx_surface_vaapi_get_display (composition->base_surface);
    subpicture->surface = gst_mfx_surface_vaapi_new (display, &info, NULL);
    gst_mfx_display_unref (display);
  }
  else {
    subpicture->surface = gst_mfx_surface_new(&info);
  }
  if (!subpicture->surface)
    return FALSE;

  gst_video_overlay_rectangle_get_render_rectangle(rect,
    (gint *)& subpicture->sub_rect.x, (gint *)& subpicture->sub_rect.y,
    &subpicture->sub_rect.width, &subpicture->sub_rect.height);

  if (!gst_mfx_surface_map(subpicture->surface))
    goto error;
  if (subpicture->sub_rect.width == GST_MFX_SURFACE_WIDTH (subpicture->surface) &&
      subpicture->sub_rect.height == GST_MFX_SURFACE_HEIGHT (subpicture->surface)) {
    memcpy(gst_mfx_surface_get_plane(subpicture->surface, 0), data, map_info.size);
  }
  else {
    guint8 *plane = gst_mfx_surface_get_plane(subpicture->surface, 0);
    guint i;

    for (i = 0; i < subpicture->sub_rect.height; i++)
      memcpy(plane + i * GST_MFX_SURFACE_WIDTH (subpicture->surface) * 4,
          data + i * subpicture->sub_rect.width * 4,
          subpicture->sub_rect.width * 4);
  }
  gst_mfx_surface_unmap(subpicture->surface);

  gst_video_meta_unmap(vmeta, 0, &map_info);

  subpicture->global_alpha = gst_video_overlay_rectangle_get_global_alpha(rect);

  g_ptr_array_add(composition->subpictures, subpicture);

  return TRUE;
error:
  destroy_subpicture (subpicture);
  return FALSE;
}

static gboolean
gst_mfx_create_surfaces_from_composition(
  GstMfxSurfaceComposition * composition,
  GstVideoOverlayComposition * overlay)
{
  guint n, nb_rectangles;

  if (!overlay)
    return FALSE;

  nb_rectangles = gst_video_overlay_composition_n_rectangles(overlay);

  /* Overlay all the rectangles cantained in the overlay composition */
  for (n = 0; n < nb_rectangles; ++n) {
    GstVideoOverlayRectangle *rect =
        gst_video_overlay_composition_get_rectangle (overlay, n);

    if (!GST_IS_VIDEO_OVERLAY_RECTANGLE(rect))
      continue;

    if (!create_subpicture(composition, rect)) {
      GST_WARNING("could not create subpicture %p", rect);
      return FALSE;
    }
  }
  return TRUE;
}

void
gst_mfx_surface_composition_finalize(GstMfxSurfaceComposition * composition)
{
  gst_mfx_surface_unref (composition->base_surface);
  g_ptr_array_free (composition->subpictures, TRUE);
}

static inline const GstMfxMiniObjectClass *
gst_mfx_surface_composition_class(void)
{
  static const GstMfxMiniObjectClass GstMfxSubpictureCompositionClass = {
    sizeof(GstMfxSurfaceComposition),
    (GDestroyNotify)gst_mfx_surface_composition_finalize
  };
  return &GstMfxSubpictureCompositionClass;
}

GstMfxSurfaceComposition *
gst_mfx_surface_composition_new (GstMfxSurface * base_surface,
  GstVideoOverlayComposition * overlay)
{
  GstMfxSurfaceComposition *composition;

  g_return_val_if_fail(base_surface != NULL, NULL);
  g_return_val_if_fail(overlay != NULL, NULL);

  composition = (GstMfxSurfaceComposition *)
                  gst_mfx_mini_object_new0(gst_mfx_surface_composition_class());
  if (!composition)
    return NULL;

  composition->base_surface = gst_mfx_surface_ref (base_surface);
  composition->subpictures =
      g_ptr_array_new_with_free_func((GDestroyNotify)destroy_subpicture);
  if (!gst_mfx_create_surfaces_from_composition(composition, overlay))
    goto error;

  return composition;
error:
  gst_mfx_mini_object_unref(GST_MFX_MINI_OBJECT(composition));
  return NULL;
}

GstMfxSurfaceComposition *
gst_mfx_surface_composition_ref (GstMfxSurfaceComposition * composition)
{
  g_return_val_if_fail(composition != NULL, NULL);

  return (GstMfxSurfaceComposition *)
           gst_mfx_mini_object_ref(GST_MFX_MINI_OBJECT(composition));
}

void
gst_mfx_surface_composition_unref(GstMfxSurfaceComposition * composition)
{
  gst_mfx_mini_object_unref(GST_MFX_MINI_OBJECT(composition));
}

void
gst_mfx_surface_composition_replace(
  GstMfxSurfaceComposition ** old_composition_ptr,
  GstMfxSurfaceComposition * new_composition)
{
  g_return_if_fail(old_composition_ptr != NULL);

  gst_mfx_mini_object_replace((GstMfxMiniObject **)old_composition_ptr,
    GST_MFX_MINI_OBJECT(new_composition));
}

GstMfxSubpicture *
gst_mfx_surface_composition_get_subpicture(
  GstMfxSurfaceComposition * composition, guint index)
{
  g_return_val_if_fail(composition != NULL, NULL);
  return g_ptr_array_index(composition->subpictures, index);
}

GstMfxSurface *
gst_mfx_surface_composition_get_base_surface (
  GstMfxSurfaceComposition * composition)
{
  g_return_val_if_fail(composition != NULL, NULL);
  return composition->base_surface;
}

guint
gst_mfx_surface_composition_get_num_subpictures(
  GstMfxSurfaceComposition * composition)
{
  return composition->subpictures->len;
}
