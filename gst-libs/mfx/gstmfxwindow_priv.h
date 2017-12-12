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

#include "gstmfxminiobject.h"

G_BEGIN_DECLS

#define GST_MFX_WINDOW_CLASS(klass) \
  ((GstMfxWindowClass *)(klass))

#define GST_MFX_WINDOW_GET_CLASS(obj) \
  GST_MFX_WINDOW_CLASS(GST_MFX_MINI_OBJECT_GET_CLASS(obj))

/* GstMfxWindowClass hooks */
typedef gboolean(*GstMfxWindowCreateFunc) (GstMfxWindow * window,
    guint * width, guint * height);
typedef void(*GstMfxWindowDestroyFunc) (GstMfxWindow * window);
typedef gboolean(*GstMfxWindowShowFunc) (GstMfxWindow * window);
typedef gboolean(*GstMfxWindowHideFunc) (GstMfxWindow * window);
typedef gboolean(*GstMfxWindowGetGeometryFunc) (GstMfxWindow * window,
    gint * px, gint * py, guint * pwidth, guint * pheight);
typedef gboolean(*GstMfxWindowSetFullscreenFunc) (GstMfxWindow * window,
    gboolean fullscreen);
typedef gboolean(*GstMfxWindowResizeFunc) (GstMfxWindow * window,
  guint width, guint height);
typedef gboolean(*GstMfxWindowRenderFunc) (GstMfxWindow * window,
    GstMfxSurface * surface, const GstMfxRectangle * src_rect,
    const GstMfxRectangle * dst_rect);

#undef GST_MFX_WINDOW_ID
#define GST_MFX_WINDOW_ID(window) \
  (GST_MFX_WINDOW (window)->handle)

#undef GST_MFX_WINDOW_DISPLAY
#define GST_MFX_WINDOW_DISPLAY(window) \
  (GST_MFX_WINDOW (window)->display)

/**
 * GstMfxWindow:
 *
 * Base class for system-dependent windows.
 */
struct _GstMfxWindow
{
  /*< private >*/
  GstMfxMiniObject parent_instance;

  GstMfxDisplay *display;
  guintptr handle;

  /*< protected >*/
  guint width;
  guint height;
  guint display_width;
  guint display_height;
  guint use_foreign_window;
  guint is_fullscreen;
  guint check_geometry;
};

/**
 * GstMfxWindowClass:
 * @create: virtual function to create a window with width and height
 * @show: virtual function to show (map) a window
 * @hide: virtual function to hide (unmap) a window
 * @get_geometry: virtual function to get the current window geometry
 * @set_fullscreen: virtual function to change window fullscreen state
 * @resize: virtual function to resize a window
 * @render: virtual function to render a #GstMfxSurface into a window
 * @get_visual_id: virtual function to get the desired visual id used to
 *   create the window
 * @get_colormap: virtual function to get the desired colormap used to
 *   create the window, or the currently allocated one
 *
 * Base class for system-dependent windows.
 */
struct _GstMfxWindowClass
{
  /*< private >*/
  GstMfxMiniObjectClass parent_class;

  /*< protected >*/
  GstMfxWindowCreateFunc create;
  GstMfxWindowDestroyFunc destroy;
  GstMfxWindowShowFunc show;
  GstMfxWindowHideFunc hide;
  GstMfxWindowGetGeometryFunc get_geometry;
  GstMfxWindowSetFullscreenFunc set_fullscreen;
  GstMfxWindowResizeFunc resize;
  GstMfxWindowRenderFunc render;
};

void
gst_mfx_window_class_init(GstMfxWindowClass * klass);

GstMfxWindow *
gst_mfx_window_new_internal(const GstMfxWindowClass * window_class,
  GstMfxDisplay * display, GstMfxID handle, guint width, guint height);

#define gst_mfx_window_ref_internal(window) \
  ((gpointer)gst_mfx_mini_object_ref(GST_MFX_MINI_OBJECT(window)))

#define gst_mfx_window_unref_internal(window) \
  gst_mfx_mini_object_unref(GST_MFX_MINI_OBJECT(window))

#define gst_mfx_window_replace_internal(old_window_ptr, new_window) \
  gst_mfx_mini_object_replace((GstMfxMiniObject **)(old_window_ptr), \
  GST_MFX_MINI_OBJECT(new_window))

#undef  gst_mfx_window_ref
#define gst_mfx_window_ref(window) \
  gst_mfx_window_ref_internal((window))

#undef  gst_mfx_window_unref
#define gst_mfx_window_unref(window) \
  gst_mfx_window_unref_internal((window))

#undef  gst_mfx_window_replace
#define gst_mfx_window_replace(old_window_ptr, new_window) \
  gst_mfx_window_replace_internal((old_window_ptr), (new_window))

G_END_DECLS

#endif /* GST_MFX_WINDOW_PRIV_H */
