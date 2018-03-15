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

#include "gstmfxsurface_vaapi.h"
#include "gstmfxdisplay.h"
#include "video-format.h"

#define DEBUG 1
#include "gstmfxdebug.h"

#define GST_MFX_SURFACE_VAAPI_CAST(obj) ((GstMfxSurfaceVaapi *)(obj))

struct _GstMfxSurfaceVaapi
{
  /*< private > */
  GstMfxSurface parent_instance;

  GstMfxDisplay *display;
  VaapiImage *image;
};

G_DEFINE_TYPE (GstMfxSurfaceVaapi, gst_mfx_surface_vaapi, GST_TYPE_MFX_SURFACE);

static gboolean
gst_mfx_surface_vaapi_from_task (GstMfxSurface * surface, GstMfxTask * task)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE (surface);
  GstMfxMemoryId *mid = gst_mfx_task_get_memory_id (task);
  if (!mid)
    return FALSE;

  priv->surface.Data.MemId = mid;
  priv->surface_id = *(GstMfxID *) mid->mid;
  return TRUE;
}

static gboolean
gst_mfx_surface_vaapi_allocate (GstMfxSurface * surface, GstMfxTask * task)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE (surface);
  GstMfxSurfaceVaapi *const vaapi_surface =
      GST_MFX_SURFACE_VAAPI_CAST (surface);

  priv->has_video_memory = TRUE;

  if (task) {
    priv->context = gst_mfx_task_get_context (task);
    vaapi_surface->display = gst_mfx_context_get_device (priv->context);
    return gst_mfx_surface_vaapi_from_task (surface, task);
  } else {
    mfxFrameInfo *frame_info = &priv->surface.Info;
    guint fourcc = gst_mfx_video_format_to_va_fourcc (frame_info->FourCC);
    VASurfaceAttrib attrib;
    VAStatus sts;

    attrib.type = VASurfaceAttribPixelFormat;
    attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    attrib.value.type = VAGenericValueTypeInteger;
    attrib.value.value.i = fourcc;

    vaapi_surface->display = gst_mfx_context_get_device (priv->context);

    GST_MFX_DISPLAY_LOCK (vaapi_surface->display);
    sts = vaCreateSurfaces (GST_MFX_DISPLAY_VADISPLAY (vaapi_surface->display),
        gst_mfx_video_format_to_va_format (frame_info->FourCC),
        frame_info->Width, frame_info->Height,
        (VASurfaceID *) & priv->surface_id, 1, &attrib, 1);
    GST_MFX_DISPLAY_UNLOCK (vaapi_surface->display);
    if (!vaapi_check_status (sts, "vaCreateSurfaces ()"))
      return FALSE;

    priv->mem_id.mid = &priv->surface_id;
    priv->mem_id.info = frame_info;
    priv->surface.Data.MemId = &priv->mem_id;

    return TRUE;
  }
}

static void
gst_mfx_surface_vaapi_release (GstMfxSurface * surface)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE (surface);
  GstMfxSurfaceVaapi *const vaapi_surface =
      GST_MFX_SURFACE_VAAPI_CAST (surface);

  vaapi_image_replace (&vaapi_surface->image, NULL);

  /* Don't destroy the underlying VASurface if originally from the task allocator */
  if (!priv->task) {
    GST_MFX_DISPLAY_LOCK (vaapi_surface->display);
    vaDestroySurfaces (GST_MFX_DISPLAY_VADISPLAY (vaapi_surface->display),
        (VASurfaceID *) & priv->surface_id, 1);
    GST_MFX_DISPLAY_UNLOCK (vaapi_surface->display);
  }
}

static gboolean
gst_mfx_surface_vaapi_map (GstMfxSurface * surface)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE (surface);
  GstMfxSurfaceVaapi *const vaapi_surface =
      GST_MFX_SURFACE_VAAPI_CAST (surface);
  guint i, num_planes;
  gboolean success = TRUE;

  vaapi_surface->image = gst_mfx_surface_vaapi_derive_image (surface);
  if (!vaapi_surface->image)
    return FALSE;

  if (!vaapi_image_map (vaapi_surface->image)) {
    GST_ERROR ("Failed to map VA surface.");
    success = FALSE;
    goto done;
  }

  num_planes = vaapi_image_get_plane_count (vaapi_surface->image);
  for (i = 0; i < num_planes; i++) {
    priv->planes[i] = vaapi_image_get_plane (vaapi_surface->image, i);
    priv->pitches[i] = vaapi_image_get_pitch (vaapi_surface->image, i);
  }
  if (num_planes == 1)
    vaapi_image_get_size (vaapi_surface->image, &priv->width, &priv->height);
  else {
    priv->width = priv->pitches[0];
    priv->height =
        vaapi_image_get_offset (vaapi_surface->image, 1) / priv->width;
  }

done:
  vaapi_image_unref (vaapi_surface->image);
  return success;
}

static void
gst_mfx_surface_vaapi_unmap (GstMfxSurface * surface)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE (surface);
  GstMfxSurfaceVaapi *const vaapi_surface =
      GST_MFX_SURFACE_VAAPI_CAST (surface);
  guint i, num_planes;

  num_planes = vaapi_image_get_plane_count (vaapi_surface->image);
  for (i = 0; i < num_planes; i++) {
    priv->planes[i] = NULL;
    priv->pitches[i] = 0;
  }
  vaapi_image_unmap (vaapi_surface->image);
}

static void
gst_mfx_surface_vaapi_finalize (GObject * object)
{
  G_OBJECT_CLASS (gst_mfx_surface_vaapi_parent_class)->finalize (object);
}

static void
gst_mfx_surface_vaapi_class_init (GstMfxSurfaceVaapiClass * klass)
{
  GstMfxSurfaceClass *const surface_class = GST_MFX_SURFACE_CLASS (klass);
  GObjectClass *const object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gst_mfx_surface_vaapi_finalize;

  surface_class->allocate = gst_mfx_surface_vaapi_allocate;
  surface_class->release = gst_mfx_surface_vaapi_release;
  surface_class->map = gst_mfx_surface_vaapi_map;
  surface_class->unmap = gst_mfx_surface_vaapi_unmap;
}

static void
gst_mfx_surface_vaapi_init (GstMfxSurfaceVaapi * surface)
{
}

GstMfxSurface *
gst_mfx_surface_vaapi_new (GstMfxContext * context, const GstVideoInfo * info)
{
  GstMfxSurfaceVaapi *surface;

  g_return_val_if_fail (context != NULL, NULL);
  g_return_val_if_fail (info != NULL, NULL);

  surface = g_object_new (GST_TYPE_MFX_SURFACE_VAAPI, NULL);
  if (!surface)
    return NULL;

  return
      gst_mfx_surface_new_internal (GST_MFX_SURFACE (surface),
          context, info, NULL);
}

GstMfxSurface *
gst_mfx_surface_vaapi_new_from_task (GstMfxTask * task)
{

  GstMfxSurfaceVaapi *surface;

  g_return_val_if_fail (task != NULL, NULL);

  surface = g_object_new (GST_TYPE_MFX_SURFACE_VAAPI, NULL);
  if (!surface)
    return NULL;

  return
      gst_mfx_surface_new_internal (GST_MFX_SURFACE (surface),
          NULL, NULL, task);
}

VaapiImage *
gst_mfx_surface_vaapi_derive_image (GstMfxSurface * surface)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE (surface);
  GstMfxSurfaceVaapi *const vaapi_surface =
      GST_MFX_SURFACE_VAAPI_CAST (surface);
  VAImage va_image;
  VAStatus status;

  g_return_val_if_fail (surface != NULL, NULL);

  if (vaapi_surface->image)
    goto done;

  va_image.image_id = VA_INVALID_ID;
  va_image.buf = VA_INVALID_ID;

  GST_MFX_DISPLAY_LOCK (vaapi_surface->display);
  status = vaDeriveImage (GST_MFX_DISPLAY_VADISPLAY (vaapi_surface->display),
      priv->surface_id, &va_image);
  GST_MFX_DISPLAY_UNLOCK (vaapi_surface->display);
  if (!vaapi_check_status (status, "vaDeriveImage ()"))
    return NULL;

  vaapi_surface->image = vaapi_image_new_with_image (vaapi_surface->display, &va_image);
  if (!vaapi_surface->image)
    return NULL;

done:
  return vaapi_image_ref (vaapi_surface->image);
}

GstMfxDisplay *
gst_mfx_surface_vaapi_get_display (GstMfxSurface * surface)
{
  g_return_val_if_fail (surface != NULL, NULL);

  return gst_mfx_display_ref (GST_MFX_SURFACE_VAAPI_CAST (surface)->display);
}
