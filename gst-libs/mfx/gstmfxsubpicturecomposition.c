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
#include "gstmfxsubpicturecomposition.h"
#include "gstmfxsurface.h"
#include "gstmfxsurface_vaapi.h"

#define DEBUG 1
#include "gstmfxdebug.h"

struct _GstMfxSubpictureComposition
{
  /*< private > */
  GstMfxMiniObject parent_instance;

  GPtrArray *subpictures;
};

static void
destroy_subpicture (GstMfxSubpicture * subpicture)
{
  gst_mfx_surface_unref(subpicture->surface);
  g_slice_free(GstMfxSubpicture, subpicture);
}

static gboolean
create_subpicture (GstMfxSubpictureComposition * composition,
  GstMfxDisplay * display, GstVideoOverlayRectangle * rect,
  gboolean memtype_is_system)
{
  GstMfxSubpicture *subpicture;
  GstBuffer *buffer;
  GstVideoMeta *vmeta;
  guint8 *data;
  guint width, height, stride;
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

  if (!memtype_is_system)
    subpicture->surface = gst_mfx_surface_vaapi_new(display, &info);
  else
    subpicture->surface = gst_mfx_surface_new(&info);
  if (!subpicture->surface)
    return FALSE;

  if (!gst_mfx_surface_map(subpicture->surface))
    goto error;
  memcpy(gst_mfx_surface_get_plane(subpicture->surface, 0), data, map_info.size);
  gst_mfx_surface_unmap(subpicture->surface);

  gst_video_overlay_rectangle_get_render_rectangle(rect,
    (gint *)& subpicture->sub_rect.x, (gint *)& subpicture->sub_rect.y,
    &subpicture->sub_rect.width, &subpicture->sub_rect.height);

  /* ensure that the overlay is not bigger than the surface */
  subpicture->sub_rect.y =
      MIN(subpicture->sub_rect.y, GST_MFX_SURFACE_HEIGHT(subpicture->surface));
  subpicture->sub_rect.width =
      MIN(subpicture->sub_rect.width, GST_MFX_SURFACE_WIDTH(subpicture->surface));

  gst_video_meta_unmap(vmeta, 0, &map_info);

  subpicture->global_alpha = gst_video_overlay_rectangle_get_global_alpha(rect);

  g_ptr_array_add(composition->subpictures, subpicture);

  return TRUE;
error:
  destroy_subpicture (subpicture);
}

static gboolean
gst_mfx_create_surfaces_from_composition(
  GstMfxSubpictureComposition * composition,
  GstVideoOverlayComposition * overlay,
  GstMfxDisplay * display, gboolean memtype_is_system)
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

    if (!create_subpicture(composition, display, rect, memtype_is_system)) {
      GST_WARNING("could not create subpicture %p", rect);
      return FALSE;
    }
  }
  return TRUE;
}

void
gst_mfx_subpicture_composition_finalize(GstMfxSubpictureComposition * composition)
{
  g_ptr_array_free (composition->subpictures, TRUE);
}

static inline const GstMfxMiniObjectClass *
gst_mfx_subpicture_composition_class(void)
{
  static const GstMfxMiniObjectClass GstMfxSubpictureCompositionClass = {
    sizeof(GstMfxSubpictureComposition),
    (GDestroyNotify)gst_mfx_subpicture_composition_finalize
  };
  return &GstMfxSubpictureCompositionClass;
}

GstMfxSubpictureComposition *
gst_mfx_subpicture_composition_new(GstMfxDisplay * display,
  GstVideoOverlayComposition * overlay,
  GstVideoInfo * info, gboolean memtype_is_system)
{
  GstMfxSubpictureComposition *composition;

  g_return_val_if_fail(composition != NULL, NULL);

  composition = gst_mfx_mini_object_new0(gst_mfx_subpicture_composition_class());
  if (!composition)
    return NULL;

  composition->subpictures =
      g_ptr_array_new_with_free_func((GDestroyNotify)destroy_subpicture);
  if (!gst_mfx_create_surfaces_from_composition(composition,
      overlay, display, memtype_is_system))
    goto error;

  return composition;
error:
  gst_mfx_mini_object_unref(composition);
  return NULL;
}

GstMfxSubpictureComposition *
gst_mfx_subpicture_composition_ref (GstMfxSubpictureComposition * composition)
{
  g_return_val_if_fail(composition != NULL, NULL);

  return gst_mfx_mini_object_ref(GST_MFX_MINI_OBJECT(composition));
}

void
gst_mfx_subpicture_composition_unref(GstMfxSubpictureComposition * composition)
{
  gst_mfx_mini_object_unref(GST_MFX_MINI_OBJECT(composition));
}

void
gst_mfx_subpicture_composition_replace(
  GstMfxSubpictureComposition ** old_composition_ptr,
  GstMfxSubpictureComposition * new_composition)
{
  g_return_if_fail(old_composition_ptr != NULL);

  gst_mfx_mini_object_replace((GstMfxMiniObject **)old_composition_ptr,
    GST_MFX_MINI_OBJECT(new_composition));
}

GstMfxSurface *
gst_mfx_subpicture_composition_get_subpicture(
  GstMfxSubpictureComposition * composition, guint index)
{
  GstMfxSubpicture *subpicture;

  g_return_val_if_fail(composition != NULL, NULL);

  subpicture = g_ptr_array_index(composition->subpictures, index);

  return subpicture;
}

void
gst_mfx_subpicture_composition_add_surface(GstMfxSubpictureComposition * composition,
  GstMfxSurface * surface, gfloat global_alpha)
{
  GstMfxSubpicture *subpicture;

  g_return_if_fail(composition != NULL);
  g_return_if_fail(surface != NULL);

  subpicture = g_slice_new0(GstMfxSubpicture);

  subpicture->surface = gst_mfx_surface_ref(surface);
  subpicture->global_alpha = global_alpha;

  g_ptr_array_add (composition->subpictures, subpicture);
}

guint
gst_mfx_subpicture_composition_get_num_subpictures(
  GstMfxSubpictureComposition * composition)
{
  return composition->subpictures->len;
}
