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

#ifndef GST_MFX_SURFACE_PRIV_H
#define GST_MFX_SURFACE_PRIV_H

#include "gstmfxsurface.h"
#include "gstmfxminiobject.h"

G_BEGIN_DECLS

#define GST_MFX_SURFACE_CLASS(klass) \
  ((GstMfxSurfaceClass *)(klass))

#define GST_MFX_SURFACE_GET_CLASS(obj) \
  GST_MFX_SURFACE_CLASS(GST_MFX_MINI_OBJECT_GET_CLASS(obj))

typedef gboolean(*GstMfxSurfaceAllocateFunc) (GstMfxSurface * surface, GstMfxTask * task);
typedef void(*GstMfxSurfaceReleaseFunc) (GstMfxSurface * surface);
typedef gboolean(*GstMfxSurfaceMapFunc) (GstMfxSurface * surface);
typedef void(*GstMfxSurfaceUnmapFunc) (GstMfxSurface * surface);

struct _GstMfxSurface
{
  /*< private >*/
  GstMfxMiniObject parent_instance;

  GstMfxDisplay *display;
  GstMfxTask *task;
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
  gboolean has_video_memory;
  mfxExtVPPVideoSignalInfo siginfo;
  mfxExtBuffer **ext_buf;
  guint queued;

  gint gem_bo_handle;
  gboolean is_gem_linear;
  gint drm_fd;
};

struct _GstMfxSurfaceClass
{
  /*< private >*/
  GstMfxMiniObjectClass parent_class;

  /*< protected >*/
  GstMfxSurfaceAllocateFunc allocate;
  GstMfxSurfaceReleaseFunc release;
  GstMfxSurfaceMapFunc map;
  GstMfxSurfaceUnmapFunc unmap;
};

GstMfxSurface *
gst_mfx_surface_new_internal(const GstMfxSurfaceClass * klass,
    GstMfxDisplay * display, const GstVideoInfo * info, GstMfxTask * task,
    gboolean is_linear, gint dri_fd);

#define gst_mfx_surface_ref_internal(surface) \
  ((gpointer)gst_mfx_mini_object_ref(GST_MFX_MINI_OBJECT(surface)))

#define gst_mfx_surface_unref_internal(surface) \
  gst_mfx_mini_object_unref(GST_MFX_MINI_OBJECT(surface))

#define gst_mfx_surface_replace_internal(old_surface_ptr, new_surface) \
  gst_mfx_mini_object_replace((GstMfxMiniObject **)(old_surface_ptr), \
  GST_MFX_MINI_OBJECT(new_surface))

#undef  gst_mfx_surface_ref
#define gst_mfx_surface_ref(surface) \
  gst_mfx_surface_ref_internal((surface))

#undef  gst_mfx_surface_unref
#define gst_mfx_surface_unref(surface) \
  gst_mfx_surface_unref_internal((surface))

#undef  gst_mfx_surface_replace
#define gst_mfx_surface_replace(old_surface_ptr, new_surface) \
  gst_mfx_surface_replace_internal((old_surface_ptr), (new_surface))

G_END_DECLS

#endif /* GST_MFX_SURFACE_PRIV_H */
