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

G_BEGIN_DECLS

#define GST_MFX_SURFACE_CLASS(klass) \
  ((GstMfxSurfaceClass *)(klass))
#define GST_MFX_SURFACE_GET_PRIVATE(surface) \
  (GST_MFX_SURFACE (surface)->priv)
#define GST_MFX_SURFACE_GET_CLASS(obj) \
  GST_MFX_SURFACE_CLASS(GST_OBJECT_GET_CLASS(obj))

typedef struct _GstMfxSurfaceClass GstMfxSurfaceClass;
typedef struct _GstMfxSurfacePrivate GstMfxSurfacePrivate;

struct _GstMfxSurfacePrivate
{
  GstMfxSurface *parent;

  GstMfxContext *context;
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
};

struct _GstMfxSurface
{
  /*< private > */
  GstObject parent_instance;

  GstMfxSurfacePrivate *priv;
};

typedef gboolean (*GstMfxSurfaceAllocateFunc) (GstMfxSurface * surface, GstMfxTask * task);
typedef void (*GstMfxSurfaceReleaseFunc) (GstMfxSurface * surface);
typedef gboolean (*GstMfxSurfaceMapFunc) (GstMfxSurface * surface);
typedef void (*GstMfxSurfaceUnmapFunc) (GstMfxSurface * surface);

struct _GstMfxSurfaceClass
{
  /*< private > */
  GstObjectClass parent_class;

  /*< protected > */
  GstMfxSurfaceAllocateFunc allocate;
  GstMfxSurfaceReleaseFunc release;
  GstMfxSurfaceMapFunc map;
  GstMfxSurfaceUnmapFunc unmap;
};

GstMfxSurface *
gst_mfx_surface_new_internal (GstMfxSurface * surface,
    GstMfxContext * context, const GstVideoInfo * info, GstMfxTask * task);

G_END_DECLS
#endif /* GST_MFX_SURFACE_PRIV_H */
