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
#include "gstmfxtask.h"
#include "gstmfxdisplay.h"

#define DEBUG 1
#include "gstmfxdebug.h"

/* Ensure those symbols are actually defined in the resulting libraries */
#undef gst_mfx_surface_ref
#undef gst_mfx_surface_unref
#undef gst_mfx_surface_replace


static gboolean
gst_mfx_surface_allocate_default (GstMfxSurface * surface, GstMfxTask * task)
{
  mfxFrameData *ptr = &surface->surface.Data;
  mfxFrameInfo *info = &surface->surface.Info;
  guint frame_size, offset = 0;
  gboolean success = TRUE;

  frame_size = info->Width * info->Height;

#ifdef WITH_MSS_2016
  /* This offset value is required for Haswell when using MFX surfaces in
   * system memory. Don't ask me why... */
  offset = 1;
#endif

  switch (info->FourCC) {
  case MFX_FOURCC_NV12:
    surface->data_size = frame_size * 3 / 2;
    surface->data = g_slice_alloc(surface->data_size + offset);
    if (!surface->data)
      goto error;
    ptr->Pitch = surface->pitches[0] = surface->pitches[1] = info->Width;

    surface->planes[0] = ptr->Y = surface->data + offset;
    surface->planes[1] = ptr->UV = ptr->Y + frame_size;

    break;
  case MFX_FOURCC_YV12:
    surface->data_size = frame_size * 3 / 2;
    surface->data = g_slice_alloc(surface->data_size);
    if (!surface->data)
      goto error;
    ptr->Pitch = surface->pitches[0] = info->Width;
    surface->pitches[1] = surface->pitches[2] = ptr->Pitch / 2;

    surface->planes[0] = ptr->Y = surface->data;
    if (surface->format == GST_VIDEO_FORMAT_I420) {
      surface->planes[1] = ptr->U = ptr->Y + frame_size;
      surface->planes[2] = ptr->V = ptr->U + (frame_size / 4);
    }
    else {
      surface->planes[1] = ptr->V = ptr->Y + frame_size;
      surface->planes[2] = ptr->U = ptr->V + (frame_size / 4);
    }

    break;
  case MFX_FOURCC_YUY2:
    surface->data_size = frame_size * 2;
    surface->data = g_slice_alloc(surface->data_size + offset);
    if (!surface->data)
      goto error;
    ptr->Pitch = surface->pitches[0] = info->Width * 2;

    surface->planes[0] = ptr->Y = surface->data + offset;
    ptr->U = ptr->Y + 1;
    ptr->V = ptr->Y + 3;

    break;
  case MFX_FOURCC_UYVY:
    surface->data_size = frame_size * 2;
    surface->data = g_slice_alloc(surface->data_size);
    if (!surface->data)
      goto error;
    ptr->Pitch = surface->pitches[0] = info->Width * 2;

    surface->planes[0] = ptr->U = surface->data;
    ptr->Y = ptr->U + 1;
    ptr->V = ptr->U + 2;

    break;
  case MFX_FOURCC_RGB4:
    surface->data_size = frame_size * 4;
    surface->data = g_slice_alloc(surface->data_size + offset);
    if (!surface->data)
      goto error;
    ptr->Pitch = surface->pitches[0] = info->Width * 4;

    surface->planes[0] = ptr->B = surface->data + offset;
    ptr->G = ptr->B + 1;
    ptr->R = ptr->B + 2;
    ptr->A = ptr->B + 3;

    break;
  case MFX_FOURCC_P010:
    surface->data_size = frame_size * 3;
    surface->data = g_slice_alloc(surface->data_size + offset);
    if (!surface->data)
      goto error;
    ptr->Pitch = surface->pitches[0] = surface->pitches[1] = info->Width * 2;

    surface->planes[0] = ptr->Y = surface->data + offset;
    surface->planes[1] = ptr->UV = ptr->Y + frame_size * 2;

    break;
  default:
error:
      GST_ERROR("Failed to create surface.");
      success = FALSE;
      break;
  }

  surface->has_video_memory = FALSE;
  return success;
}

static void
gst_mfx_surface_release_default (GstMfxSurface * surface)
{
  mfxFrameData *ptr = &surface->surface.Data;

  if (NULL != ptr) {
    ptr->Pitch = 0;
    if (surface->data)
      g_slice_free1(surface->data_size, surface->data);
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
  mfxFrameInfo *frame_info = &surface->surface.Info;

  frame_info->FourCC =
    gst_video_format_to_mfx_fourcc(GST_VIDEO_INFO_FORMAT(info));
  frame_info->PicStruct =
    GST_VIDEO_INFO_IS_INTERLACED(info) ? (GST_VIDEO_INFO_FLAG_IS_SET(info,
      GST_VIDEO_FRAME_FLAG_TFF) ? MFX_PICSTRUCT_FIELD_TFF :
      MFX_PICSTRUCT_FIELD_BFF)
    : MFX_PICSTRUCT_PROGRESSIVE;

  switch (info->finfo->format) {
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_I420:
      frame_info->ChromaFormat = MFX_CHROMAFORMAT_YUV420;
      break;
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_UYVY:
      frame_info->ChromaFormat = MFX_CHROMAFORMAT_YUV422;
      break;
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_BGRx:
      frame_info->ChromaFormat = MFX_CHROMAFORMAT_YUV444;
      break;
    default:
      g_assert_not_reached();
      break;
  }

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
  mfxFrameInfo *info = &surface->surface.Info;
  mfxFrameData *ptr = &surface->surface.Data;

  surface->width = info->Width;
  surface->height = info->Height;

  surface->crop_rect.x = info->CropX;
  surface->crop_rect.y = info->CropY;
  surface->crop_rect.width = info->CropW;
  surface->crop_rect.height = info->CropH;

#if MSDK_CHECK_VERSION(1,19)
  /* Full color range */
  surface->siginfo.Header.BufferId = MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO;
  surface->siginfo.Header.BufferSz = sizeof (mfxExtVPPVideoSignalInfo);
  surface->siginfo.TransferMatrix = MFX_TRANSFERMATRIX_UNKNOWN;
  surface->siginfo.NominalRange = MFX_NOMINALRANGE_0_255;

  if (NULL == surface->ext_buf) {
    surface->ext_buf = g_slice_alloc (sizeof (mfxExtBuffer *));
    if (NULL != surface->ext_buf) {
      surface->ext_buf[0] = (mfxExtBuffer *) &surface->siginfo;
      ptr->NumExtParam = 1;
      ptr->ExtParam = &surface->ext_buf[0];
    }
  }
#endif

  surface->queued = 0;
}

static gboolean
gst_mfx_surface_create(GstMfxSurface * surface, const GstVideoInfo * info,
    GstMfxTask * task)
{
  if (task) {
    mfxFrameAllocRequest *req = gst_mfx_task_get_request(task);
    if (!req)
      return FALSE;
    surface->surface.Info = req->Info;
    surface->format = gst_video_format_from_mfx_fourcc (req->Info.FourCC);
    surface->task = gst_mfx_task_ref (task);
  }
  else if (info) {
    gst_mfx_surface_derive_mfx_frame_info(surface, info);
    surface->format = GST_VIDEO_INFO_FORMAT(info);
  }

  if (!GST_MFX_SURFACE_GET_CLASS(surface)->allocate(surface, task))
    return FALSE;

  gst_mfx_surface_init_properties(surface);
  return TRUE;
}

static void
gst_mfx_surface_finalize (GstMfxSurface * surface)
{
  GstMfxSurfaceClass *klass = GST_MFX_SURFACE_GET_CLASS(surface);

  if (surface->ext_buf)
    g_slice_free (mfxExtBuffer *, surface->ext_buf);
  if (klass->release)
    klass->release(surface);
  gst_mfx_display_replace(&surface->display, NULL);
  gst_mfx_task_replace (&surface->task, NULL);
}

void
gst_mfx_surface_class_init(GstMfxSurfaceClass * klass)
{
  GstMfxMiniObjectClass *const object_class = GST_MFX_MINI_OBJECT_CLASS(klass);
  GstMfxSurfaceClass *const surface_class = GST_MFX_SURFACE_CLASS(klass);

  GST_DEBUG_CATEGORY_INIT(gst_debug_mfx, "mfx", 0, "MFX helper");

  object_class->size = sizeof(GstMfxSurface);
  object_class->finalize = (GDestroyNotify)gst_mfx_surface_finalize;
  surface_class->allocate = gst_mfx_surface_allocate_default;
  surface_class->release = gst_mfx_surface_release_default;
}

static inline const GstMfxSurfaceClass *
gst_mfx_surface_class(void)
{
  static GstMfxSurfaceClass g_class;
  static gsize g_class_init = FALSE;

  if (g_once_init_enter(&g_class_init)) {
    gst_mfx_surface_class_init(&g_class);
    g_once_init_leave(&g_class_init, TRUE);
  }
  return &g_class;
}

GstMfxSurface *
gst_mfx_surface_new (const GstVideoInfo * info)
{
  return gst_mfx_surface_new_internal(gst_mfx_surface_class(), NULL, info, NULL,
            FALSE, -1);
}

GstMfxSurface *
gst_mfx_surface_new_from_task (GstMfxTask * task)
{
  return gst_mfx_surface_new_internal(gst_mfx_surface_class(), NULL, NULL, task,
            FALSE, -1);
}

GstMfxSurface *
gst_mfx_surface_new_from_pool(GstMfxSurfacePool * pool)
{
  g_return_val_if_fail(pool != NULL, NULL);

  return gst_mfx_surface_pool_get_surface(pool);
}

GstMfxSurface *
gst_mfx_surface_new_internal(const GstMfxSurfaceClass * klass,
    GstMfxDisplay * display, const GstVideoInfo * info, GstMfxTask * task,
    gboolean is_linear, gint dri_fd)
{
  GstMfxSurface *surface;

  surface = (GstMfxSurface *)
    gst_mfx_mini_object_new0(GST_MFX_MINI_OBJECT_CLASS(klass));
  if (!surface)
    return NULL;

  surface->gem_bo_handle = -1;
  surface->is_gem_linear = is_linear;
  surface->drm_fd = dri_fd;

  surface->surface_id = GST_MFX_ID_INVALID;
  if (display)
    surface->display = gst_mfx_display_ref(display);

  if (!gst_mfx_surface_create(surface, info, task))
    goto error;
  return surface;

error:
  gst_mfx_surface_unref_internal(surface);
  return NULL;
}

GstMfxSurface *
gst_mfx_surface_copy(GstMfxSurface * surface)
{
  GstMfxSurface *copy;

  g_return_val_if_fail(surface != NULL, NULL);

  copy = (GstMfxSurface *)
    gst_mfx_mini_object_new0(GST_MFX_MINI_OBJECT_CLASS(gst_mfx_surface_class()));
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
gst_mfx_surface_ref(GstMfxSurface * surface)
{
  g_return_val_if_fail(surface != NULL, NULL);

  return (GstMfxSurface *) gst_mfx_mini_object_ref(GST_MFX_MINI_OBJECT(surface));
}

void
gst_mfx_surface_unref(GstMfxSurface * surface)
{
  gst_mfx_mini_object_unref(GST_MFX_MINI_OBJECT(surface));
}

void
gst_mfx_surface_replace(GstMfxSurface ** old_surface_ptr,
  GstMfxSurface * new_surface)
{
  g_return_if_fail(old_surface_ptr != NULL);

  gst_mfx_mini_object_replace((GstMfxMiniObject **)old_surface_ptr,
    GST_MFX_MINI_OBJECT(new_surface));
}

mfxFrameSurface1 *
gst_mfx_surface_get_frame_surface(GstMfxSurface * surface)
{
  g_return_val_if_fail(surface != NULL, NULL);

  return &surface->surface;
}

GstMfxID
gst_mfx_surface_get_id(GstMfxSurface * surface)
{
  g_return_val_if_fail(surface != NULL, GST_MFX_ID_INVALID);

  return surface->surface_id;
}

GstVideoFormat
gst_mfx_surface_get_format(GstMfxSurface * surface)
{
  g_return_val_if_fail(surface != NULL, 0);

  return surface->format;
}

guint
gst_mfx_surface_get_width(GstMfxSurface * surface)
{
  g_return_val_if_fail(surface != NULL, 0);

  return surface->width;
}

guint
gst_mfx_surface_get_height(GstMfxSurface * surface)
{
  g_return_val_if_fail(surface != NULL, 0);

  return surface->height;
}

void
gst_mfx_surface_get_size(GstMfxSurface * surface,
    guint * width_ptr, guint * height_ptr)
{
  g_return_if_fail(surface != NULL);

  if (width_ptr)
    *width_ptr = surface->width;

  if (height_ptr)
    *height_ptr = surface->height;
}

guint8 *
gst_mfx_surface_get_plane(GstMfxSurface * surface, guint plane)
{
  g_return_val_if_fail(surface != NULL, NULL);
  g_return_val_if_fail(plane < 3, NULL);

  return surface->planes[plane];
}

guint16
gst_mfx_surface_get_pitch(GstMfxSurface * surface, guint plane)
{
  g_return_val_if_fail(surface != NULL, 0);
  g_return_val_if_fail(plane < 3, 0);

  return surface->pitches[plane];
}

GstMfxRectangle *
gst_mfx_surface_get_crop_rect(GstMfxSurface * surface)
{
  g_return_val_if_fail(surface != NULL, NULL);

  return &surface->crop_rect;
}

gboolean
gst_mfx_surface_has_video_memory(GstMfxSurface * surface)
{
  g_return_val_if_fail(surface != NULL, FALSE);

  return surface->has_video_memory;
}

gboolean
gst_mfx_surface_map(GstMfxSurface * surface)
{
  GstMfxSurfaceClass *const klass = GST_MFX_SURFACE_GET_CLASS(surface);

  if (gst_mfx_surface_has_video_memory(surface) && !surface->mapped)
    if (klass->map)
      return (surface->mapped = klass->map(surface));

  return TRUE;
}

void
gst_mfx_surface_unmap(GstMfxSurface * surface)
{
  GstMfxSurfaceClass *const klass = GST_MFX_SURFACE_GET_CLASS(surface);

  if (gst_mfx_surface_has_video_memory(surface) && surface->mapped)
    if (klass->unmap) {
      klass->unmap(surface);
      surface->mapped = FALSE;
    }
}

gboolean
gst_mfx_surface_is_queued(GstMfxSurface * surface)
{
  if (!surface)
    return FALSE;

  return g_atomic_int_get(&surface->queued)? TRUE : FALSE;
}

void
gst_mfx_surface_queue(GstMfxSurface * surface)
{
  if (surface)
    g_atomic_int_set(&surface->queued, 1);
}

void
gst_mfx_surface_dequeue(GstMfxSurface * surface)
{
  if (surface)
    g_atomic_int_set(&surface->queued, 0);
}
