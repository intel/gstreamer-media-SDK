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

#include "gstmfxsurface.h"
#include "gstmfxsurfacepool.h"
#include "gstmfxutils_vaapi.h"

#define DEBUG 1
#include "gstmfxdebug.h"

struct _GstMfxSurface
{
  /*< private > */
  GstMfxMiniObject parent_instance;

  GstMfxDisplay *display;
  GstMfxMemoryId mem_id;
  GstMfxID surface_id;

  mfxFrameSurface1 surface;
  GstVideoFormat format;
  GstMfxRectangle crop_rect;
  guint width;
  guint height;
  guint data_size;
  guint8 *data;
  guchar *planes[3];
  guint16 pitches[3];
  gboolean mapped;
};

static inline gboolean
has_mss_driver (GstMfxDisplay * display)
{
  return !strncmp (gst_mfx_display_get_vendor_string (display), "16.4.4", 6);
}

static gboolean
gst_mfx_surface_map (GstMfxSurface * surface)
{
  mfxFrameData *ptr = &surface->surface.Data;
  mfxFrameInfo *info = &surface->surface.Info;
  guint frame_size, offset;
  gboolean success = TRUE;

  frame_size = info->Width * info->Height;
  offset = has_mss_driver (surface->display) ? 1 : 0;

  switch (info->FourCC) {
    case MFX_FOURCC_NV12:
      surface->data_size = frame_size * 3 / 2;
      surface->data = g_slice_alloc (surface->data_size + offset);
      if (!surface->data)
        goto error;
      ptr->Pitch = surface->pitches[0] = surface->pitches[1] = info->Width;

      surface->planes[0] = ptr->Y = surface->data + offset;
      surface->planes[1] = ptr->UV = ptr->Y + frame_size;

      break;
    case MFX_FOURCC_YV12:
      surface->data_size = frame_size * 3 / 2;
      surface->data = g_slice_alloc (surface->data_size);
      if (!surface->data)
        goto error;
      ptr->Pitch = surface->pitches[0] = info->Width;
      surface->pitches[1] = surface->pitches[2] = ptr->Pitch / 2;

      surface->planes[0] = ptr->Y = surface->data;
      if (surface->format == GST_VIDEO_FORMAT_I420) {
        surface->planes[1] = ptr->U = ptr->Y + frame_size;
        surface->planes[2] = ptr->V = ptr->U + (frame_size / 4);
      } else {
        surface->planes[1] = ptr->V = ptr->Y + frame_size;
        surface->planes[2] = ptr->U = ptr->V + (frame_size / 4);
      }

      break;
    case MFX_FOURCC_YUY2:
      surface->data_size = frame_size * 2;
      surface->data = g_slice_alloc (surface->data_size + offset);
      if (!surface->data)
        goto error;
      ptr->Pitch = surface->pitches[0] = info->Width * 2;

      surface->planes[0] = ptr->Y = surface->data + offset;
      ptr->U = ptr->Y + 1;
      ptr->V = ptr->Y + 3;

      break;
    case MFX_FOURCC_UYVY:
      surface->data_size = frame_size * 2;
      surface->data = g_slice_alloc (surface->data_size);
      if (!surface->data)
        goto error;
      ptr->Pitch = surface->pitches[0] = info->Width * 2;

      surface->planes[0] = ptr->U = surface->data;
      ptr->Y = ptr->U + 1;
      ptr->V = ptr->U + 2;

      break;
    case MFX_FOURCC_RGB4:
      surface->data_size = frame_size * 4;
      surface->data = g_slice_alloc (surface->data_size + offset);
      if (!surface->data)
        goto error;
      ptr->Pitch = surface->pitches[0] = info->Width * 4;

      surface->planes[0] = ptr->B = surface->data + offset;
      ptr->G = ptr->B + 1;
      ptr->R = ptr->B + 2;
      ptr->A = ptr->B + 3;

      break;
    default:
error:
      GST_ERROR ("Failed to create surface.");
      success = FALSE;
      break;
  }

  return success;
}

static void
gst_mfx_surface_unmap (GstMfxSurface * surface)
{
  mfxFrameData *ptr = &surface->surface.Data;

  if (NULL != ptr) {
    ptr->Pitch = 0;
    if (surface->data)
      g_slice_free1 (surface->data_size, surface->data);
    ptr->Y = NULL;
    ptr->U = NULL;
    ptr->V = NULL;
    ptr->A = NULL;
  }
}

static gboolean
mfx_surface_create_from_task (GstMfxSurface * surface,
    GstMfxTask * task)
{
  mfxFrameAllocRequest *req;
  req = gst_mfx_task_get_request (task);
  if (!req)
    return FALSE;

  surface->surface.Info = req->Info;

  if (gst_mfx_task_has_mapped_surface (task)) {
    gst_mfx_surface_map (surface);
    surface->mapped = TRUE;
  } else {
    GstMfxMemoryId *mid;

    mid = gst_mfx_task_get_memory_id (task);
    if (!mid)
      return FALSE;

    surface->surface.Data.MemId = mid;
    surface->mapped = FALSE;
  }

  return TRUE;
}

static void
gst_mfx_surface_derive_mfx_frame_info (GstMfxSurface * surface,
    GstVideoInfo * info)
{
  mfxFrameInfo *frame_info = &surface->surface.Info;

  frame_info->ChromaFormat = MFX_CHROMAFORMAT_YUV420;
  frame_info->FourCC =
      gst_video_format_to_mfx_fourcc (GST_VIDEO_INFO_FORMAT (info));
  frame_info->PicStruct =
      GST_VIDEO_INFO_IS_INTERLACED (info) ? (GST_VIDEO_INFO_FLAG_IS_SET (info,
          GST_VIDEO_FRAME_FLAG_TFF) ? MFX_PICSTRUCT_FIELD_TFF :
      MFX_PICSTRUCT_FIELD_BFF)
      : MFX_PICSTRUCT_PROGRESSIVE;

  frame_info->CropX = 0;
  frame_info->CropY = 0;
  frame_info->CropW = info->width;
  frame_info->CropH = info->height;
  frame_info->FrameRateExtN = info->fps_n ? info->fps_n : 30;
  frame_info->FrameRateExtD = info->fps_d;
  frame_info->AspectRatioW = info->par_n;
  frame_info->AspectRatioH = info->par_d;
  frame_info->BitDepthChroma = 8;
  frame_info->BitDepthLuma = 8;

  frame_info->Width = GST_ROUND_UP_16 (info->width);
  frame_info->Height =
      (MFX_PICSTRUCT_PROGRESSIVE == frame_info->PicStruct) ?
      GST_ROUND_UP_16 (info->height) : GST_ROUND_UP_32 (info->height);
}

static gboolean
mfx_surface_create (GstMfxSurface * surface, GstVideoInfo * info)
{
  gst_mfx_surface_derive_mfx_frame_info (surface, info);

  if (surface->mapped)
    gst_mfx_surface_map (surface);
  else {
    mfxFrameInfo *frame_info = &surface->surface.Info;
    guint fourcc = gst_mfx_video_format_to_va_fourcc (frame_info->FourCC);
    VASurfaceAttrib attrib;
    VAStatus sts;

    attrib.type = VASurfaceAttribPixelFormat;
    attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    attrib.value.type = VAGenericValueTypeInteger;
    attrib.value.value.i = fourcc;

    GST_MFX_DISPLAY_LOCK (surface->display);
    sts = vaCreateSurfaces (GST_MFX_DISPLAY_VADISPLAY (surface->display),
        gst_mfx_video_format_to_va_format (frame_info->FourCC),
        frame_info->Width, frame_info->Height,
        &surface->surface_id, 1, &attrib, 1);
    GST_MFX_DISPLAY_UNLOCK (surface->display);
    if (!vaapi_check_status (sts, "vaCreateSurfaces ()"))
      return FALSE;

    surface->mem_id.mid = &surface->surface_id;
    surface->mem_id.info = &frame_info;
    surface->surface.Data.MemId = &surface->mem_id;
  }

  return TRUE;
}

static void
gst_mfx_surface_finalize (GstMfxSurface * surface)
{
  if (surface->mapped)
    gst_mfx_surface_unmap (surface);
  else {
    if (surface->surface_id) {
      GST_MFX_DISPLAY_LOCK (surface->display);
      vaDestroySurfaces (GST_MFX_DISPLAY_VADISPLAY (surface->display),
          &surface->surface_id, 1);
      GST_MFX_DISPLAY_UNLOCK (surface->display);
    }
  }
  gst_mfx_display_replace (&surface->display, NULL);
}

static inline const GstMfxMiniObjectClass *
gst_mfx_surface_class (void)
{
  static const GstMfxMiniObjectClass GstMfxSurfaceClass = {
    sizeof (GstMfxSurface),
    (GDestroyNotify) gst_mfx_surface_finalize
  };
  return &GstMfxSurfaceClass;
}

static void
gst_mfx_surface_init_properties (GstMfxSurface * surface)
{
  mfxFrameInfo *info = &surface->surface.Info;

  surface->format = surface->format ? surface->format :
      gst_video_format_from_mfx_fourcc (info->FourCC);
  surface->width = info->Width;
  surface->height = info->Height;

  surface->crop_rect.x = info->CropX;
  surface->crop_rect.y = info->CropY;
  surface->crop_rect.width = info->CropW;
  surface->crop_rect.height = info->CropH;
}

GstMfxSurface *
gst_mfx_surface_new (GstMfxDisplay * display, GstVideoInfo * info,
    gboolean mapped)
{
  GstMfxSurface *surface;

  g_return_val_if_fail (info != NULL, NULL);

  surface = (GstMfxSurface *)
      gst_mfx_mini_object_new0 (gst_mfx_surface_class ());
  if (!surface)
    return NULL;

  surface->display = gst_mfx_display_ref (display);
  surface->mapped = mapped;
  surface->format = GST_VIDEO_INFO_FORMAT (info);

  if (!mfx_surface_create (surface, info))
    goto error;
  gst_mfx_surface_init_properties (surface);

  return surface;
error:
  gst_mfx_surface_unref (surface);
  return NULL;
}

GstMfxSurface *
gst_mfx_surface_new_from_task (GstMfxTask * task)
{
  GstMfxSurface *surface;

  g_return_val_if_fail (task != NULL, NULL);

  surface = (GstMfxSurface *)
      gst_mfx_mini_object_new0 (gst_mfx_surface_class ());
  if (!surface)
    return NULL;

  surface->display = gst_mfx_display_ref (GST_MFX_TASK_DISPLAY (task));

  if (!mfx_surface_create_from_task (surface, task))
    goto error;
  gst_mfx_surface_init_properties (surface);

  return surface;
error:
  gst_mfx_surface_unref (surface);
  return NULL;
}

GstMfxSurface *
gst_mfx_surface_new_from_pool (GstMfxSurfacePool * pool)
{
  GstMfxSurface *surface;

  g_return_val_if_fail (pool != NULL, NULL);

  surface = (GstMfxSurface *)
      gst_mfx_mini_object_new0 (gst_mfx_surface_class ());
  if (!surface)
    return NULL;

  surface = gst_mfx_surface_pool_get_surface (pool);
  if (!surface)
    return NULL;
  gst_mfx_surface_init_properties (surface);
  return surface;
}

GstMfxSurface *
gst_mfx_surface_copy (GstMfxSurface * surface)
{
  GstMfxSurface *copy;

  g_return_val_if_fail (surface != NULL, NULL);

  copy = (GstMfxSurface *)
      gst_mfx_mini_object_new0 (gst_mfx_surface_class ());
  if (!copy)
    return NULL;

  copy->surface = surface->surface;
  copy->format = surface->format;
  copy->width = surface->width;
  copy->height = surface->height;
  copy->crop_rect = surface->crop_rect;

  return copy;
}

GstMfxSurface *
gst_mfx_surface_ref (GstMfxSurface * surface)
{
  g_return_val_if_fail (surface != NULL, NULL);

  return gst_mfx_mini_object_ref (GST_MFX_MINI_OBJECT (surface));
}

void
gst_mfx_surface_unref (GstMfxSurface * surface)
{
  gst_mfx_mini_object_unref (GST_MFX_MINI_OBJECT (surface));
}

void
gst_mfx_surface_replace (GstMfxSurface ** old_surface_ptr,
    GstMfxSurface * new_surface)
{
  g_return_if_fail (old_surface_ptr != NULL);

  gst_mfx_mini_object_replace ((GstMfxMiniObject **) old_surface_ptr,
      GST_MFX_MINI_OBJECT (new_surface));
}

mfxFrameSurface1 *
gst_mfx_surface_get_frame_surface (GstMfxSurface * surface)
{
  g_return_val_if_fail (surface != NULL, NULL);

  return &surface->surface;
}

gboolean
gst_mfx_surface_is_mapped (GstMfxSurface * surface)
{
  g_return_val_if_fail (surface != NULL, NULL);

  return surface->mapped;
}

GstMfxID
gst_mfx_surface_get_id (GstMfxSurface * surface)
{
  g_return_val_if_fail (surface != NULL, GST_MFX_ID_INVALID);

  GstMfxMemoryId *mid = surface->surface.Data.MemId;

  return mid ? *(GstMfxID *) mid->mid : GST_MFX_ID_INVALID;
}

GstMfxDisplay *
gst_mfx_surface_get_display (GstMfxSurface * surface)
{
  g_return_val_if_fail (surface != NULL, GST_MFX_ID_INVALID);

  return surface->display;
}

const GstMfxRectangle *
gst_mfx_surface_get_crop_rect (GstMfxSurface * surface)
{
  g_return_val_if_fail (surface != NULL, NULL);

  return &surface->crop_rect;
}

GstVideoFormat
gst_mfx_surface_get_format (GstMfxSurface * surface)
{
  g_return_val_if_fail (surface != NULL, 0);

  return surface->format;
}

guint
gst_mfx_surface_get_width (GstMfxSurface * surface)
{
  g_return_val_if_fail (surface != NULL, 0);

  return surface->width;
}

guint
gst_mfx_surface_get_height (GstMfxSurface * surface)
{
  g_return_val_if_fail (surface != NULL, 0);

  return surface->height;
}

void
gst_mfx_surface_get_size (GstMfxSurface * surface,
    guint * width_ptr, guint * height_ptr)
{
  g_return_if_fail (surface != NULL);

  if (width_ptr)
    *width_ptr = surface->width;

  if (height_ptr)
    *height_ptr = surface->height;
}

guint8 *
gst_mfx_surface_get_data (GstMfxSurface * surface)
{
  g_return_val_if_fail (surface != NULL, NULL);

  return surface->data;
}

guint8 *
gst_mfx_surface_get_plane (GstMfxSurface * surface, guint plane)
{
  g_return_val_if_fail (surface != NULL, NULL);
  g_return_val_if_fail (plane < 3, NULL);

  return surface->planes[plane];
}

guint16
gst_mfx_surface_get_pitch (GstMfxSurface * surface, guint plane)
{
  g_return_val_if_fail (surface != NULL, NULL);
  g_return_val_if_fail (plane < 3, NULL);

  return surface->pitches[plane];
}

VaapiImage *
gst_mfx_surface_derive_image (GstMfxSurface * surface)
{
  VAImage va_image;
  VAStatus status;

  g_return_val_if_fail (surface != NULL, NULL);

  va_image.image_id = VA_INVALID_ID;
  va_image.buf = VA_INVALID_ID;

  GST_MFX_DISPLAY_LOCK (surface->display);
  status = vaDeriveImage (GST_MFX_DISPLAY_VADISPLAY (surface->display),
      GST_MFX_SURFACE_MEMID (surface), &va_image);
  GST_MFX_DISPLAY_UNLOCK (surface->display);
  if (!vaapi_check_status (status, "vaDeriveImage ()"))
    return NULL;

  if (va_image.image_id == VA_INVALID_ID || va_image.buf == VA_INVALID_ID)
    return NULL;

  return vaapi_image_new_with_image (surface->display, &va_image);
}
