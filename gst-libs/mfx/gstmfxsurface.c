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
#include "gstmfxsurface_priv.h"
#include "gstmfxsurfacepool.h"
#include "video-format.h"

#define DEBUG 1
#include "gstmfxdebug.h"

GST_DEBUG_CATEGORY(gst_debug_mfx);

G_DEFINE_TYPE_WITH_PRIVATE (GstMfxSurface,
  gst_mfx_surface,
  GST_TYPE_OBJECT);

static gboolean
gst_mfx_surface_allocate_default (GstMfxSurface * surface, GstMfxTask * task)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE(surface);

  mfxFrameData *ptr = &priv->surface.Data;
  mfxFrameInfo *info = &priv->surface.Info;
  guint frame_size;
  gboolean success = TRUE;

  frame_size = info->Width * info->Height;

  switch (info->FourCC) {
  case MFX_FOURCC_NV12:
    priv->data_size = frame_size * 3 / 2;
    priv->data = g_slice_alloc(priv->data_size);
    if (!priv->data)
      goto error;
    ptr->Pitch = priv->pitches[0] = priv->pitches[1] = info->Width;

    priv->planes[0] = ptr->Y = priv->data;
    priv->planes[1] = ptr->UV = ptr->Y + frame_size;

    break;
  case MFX_FOURCC_YV12:
    priv->data_size = frame_size * 3 / 2;
    priv->data = g_slice_alloc(priv->data_size);
    if (!priv->data)
      goto error;
    ptr->Pitch = priv->pitches[0] = info->Width;
    priv->pitches[1] = priv->pitches[2] = ptr->Pitch / 2;

    priv->planes[0] = ptr->Y = priv->data;
    if (priv->format == GST_VIDEO_FORMAT_I420) {
      priv->planes[1] = ptr->U = ptr->Y + frame_size;
      priv->planes[2] = ptr->V = ptr->U + (frame_size / 4);
    }
    else {
      priv->planes[1] = ptr->V = ptr->Y + frame_size;
      priv->planes[2] = ptr->U = ptr->V + (frame_size / 4);
    }

    break;
  case MFX_FOURCC_YUY2:
    priv->data_size = frame_size * 2;
    priv->data = g_slice_alloc(priv->data_size);
    if (!priv->data)
      goto error;
    ptr->Pitch = priv->pitches[0] = info->Width * 2;

    priv->planes[0] = ptr->Y = priv->data;
    ptr->U = ptr->Y + 1;
    ptr->V = ptr->Y + 3;

    break;
  case MFX_FOURCC_UYVY:
    priv->data_size = frame_size * 2;
    priv->data = g_slice_alloc(priv->data_size);
    if (!priv->data)
      goto error;
    ptr->Pitch = priv->pitches[0] = info->Width * 2;

    priv->planes[0] = ptr->U = priv->data;
    ptr->Y = ptr->U + 1;
    ptr->V = ptr->U + 2;

    break;
  case MFX_FOURCC_RGB4:
  case MFX_FOURCC_A2RGB10:
    priv->data_size = frame_size * 4;
    priv->data = g_slice_alloc(priv->data_size);
    if (!priv->data)
      goto error;
    ptr->Pitch = priv->pitches[0] = info->Width * 4;

    priv->planes[0] = ptr->B = priv->data;
    ptr->G = ptr->B + 1;
    ptr->R = ptr->B + 2;
    ptr->A = ptr->B + 3;

    break;
  case MFX_FOURCC_P010:
    priv->data_size = frame_size * 3;
    priv->data = g_slice_alloc(priv->data_size);
    if (!priv->data)
      goto error;
    ptr->Pitch = priv->pitches[0] = priv->pitches[1] = info->Width * 2;

    priv->planes[0] = ptr->Y = priv->data;
    priv->planes[1] = ptr->UV = ptr->Y + frame_size * 2;

    break;
  default:
error:
      GST_ERROR("Failed to create surface.");
      success = FALSE;
      break;
  }

  priv->has_video_memory = FALSE;
  return success;
}

static void
gst_mfx_surface_release_default (GObject * surface)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE(surface);
  mfxFrameData *ptr = &priv->surface.Data;

  if (NULL != ptr) {
    ptr->Pitch = 0;
    if (priv->data)
      g_slice_free1(priv->data_size, priv->data);
    ptr->Y = NULL;
    ptr->U = NULL;
    ptr->V = NULL;
    ptr->A = NULL;
  }
}

static void
gst_mfx_surface_derive_mfx_frame_info(GstMfxSurface * surface,
  const GstVideoInfo * info)
{
  mfxFrameInfo *frame_info = &(GST_MFX_SURFACE_GET_PRIVATE(surface)->surface.Info);

  frame_info->ChromaFormat = MFX_CHROMAFORMAT_YUV420;
  frame_info->FourCC =
    gst_video_format_to_mfx_fourcc(GST_VIDEO_INFO_FORMAT(info));
  frame_info->PicStruct =
    GST_VIDEO_INFO_IS_INTERLACED(info) ? (GST_VIDEO_INFO_FLAG_IS_SET(info,
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

  frame_info->Width = GST_ROUND_UP_16(info->width);
  frame_info->Height =
    (MFX_PICSTRUCT_PROGRESSIVE == frame_info->PicStruct) ?
    GST_ROUND_UP_16(info->height) : GST_ROUND_UP_32(info->height);
}

static void
gst_mfx_surface_init_properties(GstMfxSurface * surface)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE(surface);

  mfxFrameInfo *info = &priv->surface.Info;

  priv->width = info->Width;
  priv->height = info->Height;

  priv->crop_rect.x = info->CropX;
  priv->crop_rect.y = info->CropY;
  priv->crop_rect.width = info->CropW;
  priv->crop_rect.height = info->CropH;
}

static void
gst_mfx_surface_init (GstMfxSurface * surface)
{
  GstMfxSurfacePrivate *const priv =
    gst_mfx_surface_get_instance_private (surface);

  priv->surface_id = GST_MFX_ID_INVALID;
  surface->priv = priv;
}

static gboolean
gst_mfx_surface_create(GstMfxSurface * surface, const GstVideoInfo * info,
    GstMfxTask * task)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE (surface);

  if (task) {
    mfxFrameAllocRequest *req = gst_mfx_task_get_request (task);
    if (!req)
      return FALSE;
    priv->surface.Info = req->Info;
    priv->format = gst_video_format_from_mfx_fourcc (req->Info.FourCC);
    priv->task = gst_mfx_task_ref (task);
  }
  else if (info) {
    gst_mfx_surface_derive_mfx_frame_info (surface, info);
    priv->format = GST_VIDEO_INFO_FORMAT (info);
  }

  if (!GST_MFX_SURFACE_GET_CLASS(surface)->allocate (surface, task))
    return FALSE;

  gst_mfx_surface_init_properties (surface);
  return TRUE;
}

static void
gst_mfx_surface_finalize (GObject * surface)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE(surface);
  GstMfxSurfaceClass *klass = GST_MFX_SURFACE_GET_CLASS(surface);

  if (klass->release)
    klass->release (surface);
  gst_mfx_task_replace (&priv->task, NULL);
  gst_mfx_context_replace (&priv->context, NULL);
}

void
gst_mfx_surface_class_init(GstMfxSurfaceClass * klass)
{
  GObjectClass *const object_class = G_OBJECT_CLASS(klass);
  GstMfxSurfaceClass *const surface_class = GST_MFX_SURFACE_CLASS(klass);

  GST_DEBUG_CATEGORY_INIT(gst_debug_mfx, "mfx", 0, "MFX helper");

  object_class->finalize = gst_mfx_surface_finalize;

  surface_class->allocate = gst_mfx_surface_allocate_default;
  surface_class->release = gst_mfx_surface_release_default;
}

GstMfxSurface *
gst_mfx_surface_new (const GstVideoInfo * info)
{
  GstMfxSurface * surface;

  g_return_val_if_fail(info != NULL, NULL);

  surface = g_object_new(GST_TYPE_MFX_SURFACE, NULL);
  if (!surface)
    return NULL;
  return gst_mfx_surface_new_internal (surface, NULL, info, NULL);
}

GstMfxSurface *
gst_mfx_surface_new_from_task (GstMfxTask * task)
{
  GstMfxSurface * surface;

  g_return_val_if_fail(task != NULL, NULL);

  surface = g_object_new(GST_TYPE_MFX_SURFACE, NULL);
  if (!surface)
    return NULL;
  return gst_mfx_surface_new_internal (surface, NULL, NULL, task);
}

GstMfxSurface *
gst_mfx_surface_new_from_pool(GstMfxSurfacePool * pool)
{
  g_return_val_if_fail(pool != NULL, NULL);

  return gst_mfx_surface_pool_get_surface (pool);
}

GstMfxSurface *
gst_mfx_surface_new_internal(GstMfxSurface *surface, GstMfxContext * context,
  const GstVideoInfo * info, GstMfxTask * task)
{
  GST_MFX_SURFACE_GET_PRIVATE(surface)->context = context && !task ?
    gst_mfx_context_ref(context) : NULL;
  if (!gst_mfx_surface_create(surface, info, task))
    goto error;
  return surface;

error:
  gst_mfx_surface_unref (surface);
  return NULL;
}

GstMfxSurface *
gst_mfx_surface_copy(GstMfxSurface * surface)
{
  GstMfxSurface *copy;
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE(surface);
  GstMfxSurfacePrivate *copy_priv = NULL;

  g_return_val_if_fail(surface != NULL, NULL);

  copy = g_object_new(GST_TYPE_MFX_SURFACE, NULL);
  if (!copy)
    return NULL;
  copy_priv = GST_MFX_SURFACE_GET_PRIVATE(copy);

  copy_priv->surface = priv->surface;
  copy_priv->format = priv->format;
  copy_priv->width = priv->width;
  copy_priv->height = priv->height;
  copy_priv->crop_rect = priv->crop_rect;

  return copy;
}

GstMfxSurface *
gst_mfx_surface_ref(GstMfxSurface * surface)
{
  g_return_val_if_fail(surface != NULL, NULL);

  return gst_object_ref(GST_OBJECT(surface));
}

void
gst_mfx_surface_unref(GstMfxSurface * surface)
{
  gst_object_unref(GST_OBJECT(surface));
}

void
gst_mfx_surface_replace(GstMfxSurface ** old_surface_ptr,
  GstMfxSurface * new_surface)
{
  g_return_if_fail(old_surface_ptr != NULL);

  gst_object_replace((GstObject **)old_surface_ptr,
    GST_OBJECT(new_surface));
}

mfxFrameSurface1 *
gst_mfx_surface_get_frame_surface(GstMfxSurface * surface)
{
  g_return_val_if_fail(surface != NULL, NULL);

  return &GST_MFX_SURFACE_GET_PRIVATE(surface)->surface;
}

GstMfxID
gst_mfx_surface_get_id (GstMfxSurface * surface)
{
  g_return_val_if_fail(surface != NULL, GST_MFX_ID_INVALID);

  return GST_MFX_SURFACE_GET_PRIVATE(surface)->surface_id;
}

GstVideoFormat
gst_mfx_surface_get_format(GstMfxSurface * surface)
{
  g_return_val_if_fail(surface != NULL, 0);

  return GST_MFX_SURFACE_GET_PRIVATE(surface)->format;
}

guint
gst_mfx_surface_get_width(GstMfxSurface * surface)
{
  g_return_val_if_fail(surface != NULL, 0);

  return GST_MFX_SURFACE_GET_PRIVATE(surface)->width;
}

guint
gst_mfx_surface_get_height(GstMfxSurface * surface)
{
  g_return_val_if_fail(surface != NULL, 0);

  return GST_MFX_SURFACE_GET_PRIVATE(surface)->height;
}

void
gst_mfx_surface_get_size(GstMfxSurface * surface,
    guint * width_ptr, guint * height_ptr)
{
  g_return_if_fail(surface != NULL);

  if (width_ptr)
    *width_ptr = GST_MFX_SURFACE_GET_PRIVATE(surface)->width;

  if (height_ptr)
    *height_ptr = GST_MFX_SURFACE_GET_PRIVATE(surface)->height;
}

guint8 *
gst_mfx_surface_get_plane(GstMfxSurface * surface, guint plane)
{
  g_return_val_if_fail(surface != NULL, NULL);
  g_return_val_if_fail(plane < 3, NULL);

  return GST_MFX_SURFACE_GET_PRIVATE(surface)->planes[plane];
}

guint16
gst_mfx_surface_get_pitch(GstMfxSurface * surface, guint plane)
{
  g_return_val_if_fail(surface != NULL, 0);
  g_return_val_if_fail(plane < 3, 0);

  return GST_MFX_SURFACE_GET_PRIVATE(surface)->pitches[plane];
}

GstMfxRectangle *
gst_mfx_surface_get_crop_rect(GstMfxSurface * surface)
{
  g_return_val_if_fail(surface != NULL, NULL);

  return &GST_MFX_SURFACE_GET_PRIVATE(surface)->crop_rect;
}

guint
gst_mfx_surface_get_data_size(GstMfxSurface * surface)
{
  g_return_val_if_fail(surface != NULL, 0);

  return GST_MFX_SURFACE_GET_PRIVATE(surface)->data_size;
}

GstMfxContext *
gst_mfx_surface_get_context(GstMfxSurface * surface)
{
  GstMfxSurfacePrivate *const priv = GST_MFX_SURFACE_GET_PRIVATE(surface);

  return priv->context ? gst_mfx_context_ref(priv->context) : NULL;
}

gboolean
gst_mfx_surface_has_video_memory(GstMfxSurface * surface)
{
  g_return_val_if_fail(surface != NULL, FALSE);

  return GST_MFX_SURFACE_GET_PRIVATE(surface)->has_video_memory;
}

gboolean
gst_mfx_surface_map(GstMfxSurface * surface)
{
  GstMfxSurfaceClass *const klass = GST_MFX_SURFACE_GET_CLASS(surface);

  if (gst_mfx_surface_has_video_memory(surface) && !GST_MFX_SURFACE_GET_PRIVATE(surface)->mapped)
    if (klass->map)
      return (GST_MFX_SURFACE_GET_PRIVATE(surface)->mapped = klass->map(surface));

  return TRUE;
}

void
gst_mfx_surface_unmap(GstMfxSurface * surface)
{
  GstMfxSurfaceClass *const klass = GST_MFX_SURFACE_GET_CLASS(surface);

  if (gst_mfx_surface_has_video_memory(surface) && GST_MFX_SURFACE_GET_PRIVATE(surface)->mapped)
    if (klass->unmap) {
      klass->unmap(surface);
      GST_MFX_SURFACE_GET_PRIVATE(surface)->mapped = FALSE;
    }
}
