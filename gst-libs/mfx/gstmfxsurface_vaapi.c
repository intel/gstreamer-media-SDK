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
#include "gstmfxsurface_vaapi.h"
#include "gstmfxdisplay.h"
#include "gstmfxutils_vaapi.h"

#include <glib.h>
#include <xf86drm.h>
#include <libdrm/i915_drm.h>
#include <va/va_drmcommon.h>
#include <fcntl.h>

#define DEBUG 1
#include "gstmfxdebug.h"

typedef struct _GstMfxSurfaceVaapiClass GstMfxSurfaceVaapiClass;

struct _GstMfxSurfaceVaapi
{
  /*< private > */
  GstMfxSurface parent_instance;

  VaapiImage *image;
};

struct _GstMfxSurfaceVaapiClass
{
  /*< private > */
  GstMfxSurfaceClass parent_class;
};

static gboolean
gst_mfx_surface_vaapi_from_task(GstMfxSurface * surface,
    GstMfxTask * task)
{
  GstMfxMemoryId *mid = gst_mfx_task_get_memory_id(task);
  if (!mid)
    return FALSE;

  surface->surface.Data.MemId = mid;
  surface->surface_id = (GstMfxID) (*((VASurfaceID *) mid->mid));
  return TRUE;
}

static gboolean
gst_mfx_surface_vaapi_allocate(GstMfxSurface * surface, GstMfxTask * task)
{
  surface->has_video_memory = TRUE;

  if (task) {
    surface->display = gst_mfx_task_get_display (task);
    return gst_mfx_surface_vaapi_from_task(surface, task);
  }
  else {
    mfxFrameInfo *frame_info = &surface->surface.Info;
    guint fourcc = gst_mfx_video_format_to_va_fourcc(frame_info->FourCC);
    VAStatus sts;

    struct drm_i915_gem_create *drm_info = NULL;
    struct drm_i915_gem_set_tiling *tile_info = NULL;
    struct drm_i915_gem_set_domain *set_domain = NULL;
    struct drm_prime_handle *preq = NULL;
    struct drm_gem_close *close = NULL;

    if (surface->is_gem_linear && fourcc == VA_FOURCC_NV12) {
      VASurfaceAttrib attribs[2];
      VASurfaceAttribExternalBuffers external;
      unsigned long gem_handle = 0;
      int size = 0;
      int num_planes = 0;
      int pitches = 0;

      switch (fourcc) {
        case VA_FOURCC_NV12:
            size = frame_info->Width * frame_info->Height * 3 / 2;
            num_planes = 2;
            pitches = frame_info->Width;
        break;

       default:
            GST_DEBUG("Unsupported color format");
            return FALSE;
      }
      close = g_new0(struct drm_gem_close, 1);
      if (close == NULL) {
        GST_ERROR ("Failed alloc drm_gem_close.");
        goto done;
      }

      drm_info = g_new0(struct drm_i915_gem_create, 1);
      if (drm_info == NULL) {
        GST_ERROR ("Failed alloc drm_info.");
        goto done;
      }

      drm_info->size = size;
      if (drmIoctl (surface->drm_fd, DRM_IOCTL_I915_GEM_CREATE, drm_info)) {
        GST_ERROR ("Failed drmIoctl(DRM_IOCTL_I915_GEM_CREATE)");
        g_free(drm_info);
        return FALSE;
      }

      tile_info = g_new0(struct drm_i915_gem_set_tiling, 1);
      if (tile_info == NULL) {
        GST_ERROR ("Failed alloc drm_info.");
	goto done;
      }

      tile_info->handle = drm_info->handle;
      tile_info->tiling_mode = I915_TILING_NONE;
      tile_info->stride = 0;

      if (drmIoctl (surface->drm_fd, DRM_IOCTL_I915_GEM_SET_TILING, tile_info)) {
        GST_ERROR ("Failed drmIoctl(DRM_IOCTL_I915_GEM_SET_TILING)");
        goto done;
      }

      set_domain = g_new0(struct drm_i915_gem_set_domain, 1);
      if (set_domain == NULL) {
        GST_ERROR ("Failed alloc set_domain.");
        goto done;
      }

      set_domain->handle = drm_info->handle;
      set_domain->read_domains = I915_GEM_DOMAIN_GTT;
      set_domain->write_domain = I915_GEM_DOMAIN_GTT;

      if (drmIoctl (surface->drm_fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, set_domain)) {
        GST_ERROR ("Failed drmIoctl(DRM_IOCTL_I915_GEM_SET_DOMAIN)");
        goto done;
      }

      preq = g_new0(struct drm_prime_handle, 1);
      if (preq == NULL) {
        GST_ERROR ("Failed alloc prime_handle.");
        goto done;
      }

      preq->handle = drm_info->handle;
      preq->flags = O_CLOEXEC|O_RDWR;

      if (drmIoctl (surface->drm_fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, preq)) {
        GST_ERROR ("Failed drmIoctl(DRM_IOCTL_PRIME_HANDLE)");
        goto done;
      }

      gem_handle = (unsigned long) preq->fd;
      memset (&external, 0, sizeof(external));

      external.pixel_format = fourcc;
      external.width = frame_info->CropW;
      external.height = frame_info->CropH;
      external.num_planes = num_planes;
      external.data_size = size;
      external.pitches[0] = pitches;
      external.num_buffers = 1;
      external.buffers = &gem_handle;

      memset (&attribs, 0, sizeof(attribs));
      attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
      attribs[0].type = VASurfaceAttribMemoryType;
      attribs[0].value.type = VAGenericValueTypeInteger;
      attribs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;

      attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
      attribs[1].type = VASurfaceAttribExternalBufferDescriptor;
      attribs[1].value.type = VAGenericValueTypePointer;
      attribs[1].value.value.p = &external;

      GST_MFX_DISPLAY_LOCK(surface->display);
      sts = vaCreateSurfaces(GST_MFX_DISPLAY_VADISPLAY(surface->display),
       gst_mfx_video_format_to_va_format(frame_info->FourCC),
       frame_info->Width, frame_info->Height,
       (VASurfaceID *) &surface->surface_id, 1, attribs, 2);
      GST_MFX_DISPLAY_UNLOCK(surface->display);
      if (!vaapi_check_status(sts, "vaCreateSurfaces ()"))
        goto done;

      surface->mem_id.mid = &surface->surface_id;
      surface->mem_id.info = frame_info;
      surface->surface.Data.MemId = &surface->mem_id;
      surface->gem_bo_handle = drm_info->handle;

      g_free(preq);
      g_free(set_domain);
      g_free(tile_info);
      g_free(drm_info);
      g_free(close);

      return TRUE;

    } else {
      VASurfaceAttrib attrib;
      attrib.type = VASurfaceAttribPixelFormat;
      attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
      attrib.value.type = VAGenericValueTypeInteger;
      attrib.value.value.i = fourcc;

      GST_MFX_DISPLAY_LOCK(surface->display);
      sts = vaCreateSurfaces(GST_MFX_DISPLAY_VADISPLAY(surface->display),
        gst_mfx_video_format_to_va_format(frame_info->FourCC),
        frame_info->Width, frame_info->Height,
        (VASurfaceID *) &surface->surface_id, 1, &attrib, 1);
      GST_MFX_DISPLAY_UNLOCK(surface->display);
      if (!vaapi_check_status(sts, "vaCreateSurfaces ()"))
        return FALSE;

      surface->mem_id.mid = &surface->surface_id;
      surface->mem_id.info = frame_info;
      surface->surface.Data.MemId = &surface->mem_id;

      return TRUE;
    }

done:
    if (preq)
      g_free(preq);

    if (set_domain)
      g_free(set_domain);

    if (tile_info)
      g_free(tile_info);

    if (drm_info) {
      if (drm_info->handle) {
        if (close) {
          close->handle = drm_info->handle;
          if (drmIoctl(surface->drm_fd, DRM_IOCTL_GEM_CLOSE, close))
            GST_ERROR("Failed to close drm gem handle %d", drm_info->handle);
	  g_free(close);
	  close = NULL;
        }
      }
      g_free(drm_info);
    }

    if (close)
      g_free(close);

    return FALSE;
  }
}

static void
gst_mfx_surface_vaapi_close_gem_handle (GstMfxSurface * surface)
{
   if (surface->gem_bo_handle != -1) {
     struct drm_gem_close *close = NULL;
     close = g_new0(struct drm_gem_close, 1);
     if (close == NULL) {
       GST_ERROR("Failed to alloc drm_gem_close memory\n");
       return;
     }

     close->handle = surface->gem_bo_handle;
     if (drmIoctl(surface->drm_fd, DRM_IOCTL_GEM_CLOSE, close)) {
       GST_ERROR("Failed to close gem handle %d", surface->gem_bo_handle);
       return;
     }
     g_free(close);
   }
}

static void
gst_mfx_surface_vaapi_release(GstMfxSurface * surface)
{
  VAStatus status;
  /* Don't destroy the underlying VASurface if originally from the task allocator*/
  if (!surface->task) {
    gst_mfx_surface_vaapi_close_gem_handle (surface);

    GST_MFX_DISPLAY_LOCK(surface->display);
    status = vaDestroySurfaces(GST_MFX_DISPLAY_VADISPLAY(surface->display),
        (VASurfaceID *) &surface->surface_id, 1);
    if (!vaapi_check_status(status, "vaDestroySurfaces ()"))
      return;
    GST_MFX_DISPLAY_UNLOCK(surface->display);
  }
}

static gboolean
gst_mfx_surface_vaapi_map(GstMfxSurface * surface)
{
  GstMfxSurfaceVaapi *vaapi_surface = GST_MFX_SURFACE_VAAPI(surface);
  guint i, num_planes;
  gboolean success = TRUE;

  vaapi_surface->image = gst_mfx_surface_vaapi_derive_image(surface);
  if (!vaapi_surface->image)
    return FALSE;

  if (!vaapi_image_map(vaapi_surface->image)) {
    GST_ERROR ("Failed to map VA surface.");
    success = FALSE;
    goto done;
  }

  num_planes = vaapi_image_get_plane_count(vaapi_surface->image);
  for (i = 0; i < num_planes; i++) {
    surface->planes[i] = vaapi_image_get_plane(vaapi_surface->image, i);
    surface->pitches[i] = vaapi_image_get_pitch(vaapi_surface->image, i);
  }
  if (num_planes == 1)
    vaapi_image_get_size(vaapi_surface->image, &surface->width, &surface->height);
  else {
    surface->width = surface->pitches[0];
    surface->height =
        vaapi_image_get_offset(vaapi_surface->image, 1) / surface->width;
  }

done:
  vaapi_image_unref (vaapi_surface->image);
  return success;
}

static void
gst_mfx_surface_vaapi_unmap(GstMfxSurface * surface)
{
  GstMfxSurfaceVaapi *vaapi_surface = GST_MFX_SURFACE_VAAPI(surface);
  guint i, num_planes;

  num_planes = vaapi_image_get_plane_count(vaapi_surface->image);
  for (i = 0; i < num_planes; i++) {
    surface->planes[i] = NULL;
    surface->pitches[i] = 0;
  }
  vaapi_image_unmap(vaapi_surface->image);
}

void
gst_mfx_surface_vaapi_class_init(GstMfxSurfaceVaapiClass * klass)
{
  GstMfxMiniObjectClass *const object_class = GST_MFX_MINI_OBJECT_CLASS(klass);
  GstMfxSurfaceClass *const surface_class = GST_MFX_SURFACE_CLASS(klass);

  GST_DEBUG_CATEGORY_INIT(gst_debug_mfx, "mfx", 0, "MFX helper");

  gst_mfx_surface_class_init (&klass->parent_class);

  object_class->size = sizeof(GstMfxSurfaceVaapi);
  surface_class->allocate = gst_mfx_surface_vaapi_allocate;
  surface_class->release = gst_mfx_surface_vaapi_release;
  surface_class->map = gst_mfx_surface_vaapi_map;
  surface_class->unmap = gst_mfx_surface_vaapi_unmap;
}

static inline const GstMfxSurfaceClass *
gst_mfx_surface_vaapi_class(void)
{
  static GstMfxSurfaceVaapiClass g_class;
  static gsize g_class_init = FALSE;

  if (g_once_init_enter(&g_class_init)) {
    gst_mfx_surface_vaapi_class_init(&g_class);
    g_once_init_leave(&g_class_init, TRUE);
  }
  return GST_MFX_SURFACE_CLASS(&g_class);
}

GstMfxSurface *
gst_mfx_surface_vaapi_new(GstMfxDisplay * display, const GstVideoInfo * info,
    GstMfxVideoMeta *meta)
{
  gboolean is_linear = FALSE;
  gint dri_fd = -1;

  if (meta) {
    is_linear = gst_mfx_video_meta_get_linear(meta);
    dri_fd = get_display_fd(display);
  }

  return
    gst_mfx_surface_new_internal(gst_mfx_surface_vaapi_class(),
        display, info, NULL, is_linear, dri_fd);
}

GstMfxSurface *
gst_mfx_surface_vaapi_new_from_task(GstMfxTask * task)
{
  gboolean is_linear = FALSE;
  gint dri_fd = -1;
  return
    gst_mfx_surface_new_internal(gst_mfx_surface_vaapi_class(),
        NULL, NULL, task, is_linear, dri_fd);
}

GstMfxDisplay *
gst_mfx_surface_vaapi_get_display(GstMfxSurface * surface)
{
  g_return_val_if_fail(surface != NULL, NULL);

  return gst_mfx_display_ref (surface->display);
}

VaapiImage *
gst_mfx_surface_vaapi_derive_image(GstMfxSurface * surface)
{
  VAImage va_image;
  VAStatus status;

  g_return_val_if_fail(surface != NULL, NULL);

  va_image.image_id = VA_INVALID_ID;
  va_image.buf = VA_INVALID_ID;

  GST_MFX_DISPLAY_LOCK(surface->display);
  status = vaDeriveImage(GST_MFX_DISPLAY_VADISPLAY(surface->display),
    surface->surface_id, &va_image);
  GST_MFX_DISPLAY_UNLOCK(surface->display);
  if (!vaapi_check_status(status, "vaDeriveImage ()"))
    return NULL;

  if (va_image.image_id == VA_INVALID_ID || va_image.buf == VA_INVALID_ID)
    return NULL;

  return vaapi_image_new_with_image(surface->display, &va_image);
}
