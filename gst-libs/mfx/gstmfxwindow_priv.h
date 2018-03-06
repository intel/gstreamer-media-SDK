/*
 *  Copyright (C) 2012-2013 Intel Corporation
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
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

#ifndef GST_MFX_WINDOW_PRIV_H
#define GST_MFX_WINDOW_PRIV_H

G_BEGIN_DECLS
#define GST_MFX_WINDOW_CLASS(klass) \
  ((GstMfxWindowClass *)(klass))
#define GST_MFX_WINDOW_GET_PRIVATE(window) \
  (GST_MFX_WINDOW (window)->priv)
#define GST_MFX_WINDOW_GET_CLASS(obj) \
  GST_MFX_WINDOW_CLASS(GST_OBJECT_GET_CLASS(obj))
#undef GST_MFX_WINDOW_ID
#define GST_MFX_WINDOW_ID(window) \
  (GST_MFX_WINDOW_GET_PRIVATE (window)->handle)
typedef struct _GstMfxWindowClass GstMfxWindowClass;
typedef struct _GstMfxWindowPrivate GstMfxWindowPrivate;

struct _GstMfxWindowPrivate
{
  /*< private > */
  GstMfxWindow *parent;

  GstMfxContext *context;
  guintptr handle;

  /*< protected > */
  guint width;
  guint height;
  guint use_foreign_window;
  guint is_fullscreen;
  guint check_geometry;
};


/**
 * GstMfxWindow:
 *
 * Base class for system-dependent windows.
 */
struct _GstMfxWindow
{
  /*< private > */
  GstObject parent_instance;

  GstMfxWindowPrivate *priv;
};


/* GstMfxWindowClass hooks */
typedef gboolean (*GstMfxWindowCreateFunc) (GstMfxWindow * window,
    guint * width, guint * height);
typedef gboolean (*GstMfxWindowShowFunc) (GstMfxWindow * window);
typedef gboolean (*GstMfxWindowHideFunc) (GstMfxWindow * window);
typedef gboolean (*GstMfxWindowGetGeometryFunc) (GstMfxWindow * window,
    gint * px, gint * py, guint * pwidth, guint * pheight);
typedef gboolean (*GstMfxWindowSetFullscreenFunc) (GstMfxWindow * window,
    gboolean fullscreen);
typedef gboolean (*GstMfxWindowResizeFunc) (GstMfxWindow * window,
    guint width, guint height);
typedef gboolean (*GstMfxWindowRenderFunc) (GstMfxWindow * window,
    GstMfxSurface * surface, const GstMfxRectangle * src_rect,
    const GstMfxRectangle * dst_rect);

/**
 * GstMfxWindowClass:
 * @create: virtual function to create a window with width and height
 * @show: virtual function to show (map) a window
 * @hide: virtual function to hide (unmap) a window
 * @get_geometry: virtual function to get the current window geometry
 * @set_fullscreen: virtual function to change window fullscreen state
 * @resize: virtual function to resize a window
 * @render: virtual function to render a #GstMfxSurface into a window
 *
 * Base class for system-dependent windows.
 */
struct _GstMfxWindowClass
{
  /*< private > */
  GstObjectClass parent_class;

  /*< protected > */
  GstMfxWindowCreateFunc create;
  GstMfxWindowShowFunc show;
  GstMfxWindowHideFunc hide;
  GstMfxWindowGetGeometryFunc get_geometry;
  GstMfxWindowSetFullscreenFunc set_fullscreen;
  GstMfxWindowResizeFunc resize;
  GstMfxWindowRenderFunc render;
};

GstMfxWindow *
gst_mfx_window_new_internal (GstMfxWindow * window,
    GstMfxContext * context, GstMfxID id, guint width, guint height);

G_END_DECLS
#endif /* GST_MFX_WINDOW_PRIV_H */
