/*
 *  Copyright (C) 2011-2013 Intel Corporation
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

#include "sysdeps.h"
#include "gstmfxwindow.h"
#include "gstmfxwindow_priv.h"
#include "gstmfxdisplay_priv.h"
#include "gstmfxsurfaceproxy.h"

#define DEBUG 1
#include "gstmfxdebug.h"

/* Ensure those symbols are actually defined in the resulting libraries */
#undef gst_mfx_window_ref
#undef gst_mfx_window_unref
#undef gst_mfx_window_replace

static void
gst_mfx_window_ensure_size (GstMfxWindow * window)
{
  const GstMfxWindowClass *const klass = GST_MFX_WINDOW_GET_CLASS (window);
  guint width = window->width, height = window->height;

  if (!window->check_geometry)
    return;

  if (klass->get_geometry)
    klass->get_geometry (window, NULL, NULL, &window->width, &window->height);

  window->check_geometry = FALSE;
  window->is_fullscreen = (window->width == window->display_width &&
      window->height == window->display_height);

  if (width == window->width && height == window->height)
    return;

  if (klass->resize)
    klass->resize (window, window->width, window->height);
}

static gboolean
gst_mfx_window_create (GstMfxWindow * window, guint width, guint height)
{
  gst_mfx_display_get_size (GST_MFX_OBJECT_DISPLAY (window),
      &window->display_width, &window->display_height);

  if (!GST_MFX_WINDOW_GET_CLASS (window)->create (window, &width, &height))
    return FALSE;

  if (width != window->width || height != window->height) {
    GST_DEBUG ("backend resized window to %ux%u", width, height);
    window->width = width;
    window->height = height;
  }

  return TRUE;
}

GstMfxWindow *
gst_mfx_window_new_internal (const GstMfxWindowClass * window_class,
    GstMfxDisplay * display, guint width, guint height)
{
  GstMfxWindow *window;

  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);

  window =
      gst_mfx_object_new (GST_MFX_MINI_OBJECT_CLASS (window_class), display);
  if (!window)
    return NULL;

  if (!gst_mfx_window_create (window, width, height))
    goto error;
  return window;

error:
  gst_mfx_window_unref_internal (window);
  return NULL;
}

/**
 * gst_mfx_window_new:
 * @display: a #GstMfxDisplay
 * @width: the requested window width, in pixels
 * @height: the requested windo height, in pixels
 *
 * Creates a window with the specified @width and @height. The window
 * will be attached to the @display and remains invisible to the user
 * until gst_mfx_window_show () is called.
 *
 * Return value: the newly allocated #GstMfxWindow object
 */
GstMfxWindow *
gst_mfx_window_new (GstMfxDisplay * display, guint width, guint height)
{
  GstMfxDisplayClass *dpy_class;

  g_return_val_if_fail (display != NULL, NULL);

  dpy_class = GST_MFX_DISPLAY_GET_CLASS (display);
  if (G_UNLIKELY (!dpy_class->create_window))
    return NULL;
  return dpy_class->create_window (display, width, height);
}

/**
 * gst_mfx_window_ref:
 * @window: a #GstMfxWindow
 *
 * Atomically increases the reference count of the given @window by one.
 *
 * Returns: The same @window argument
 */
GstMfxWindow *
gst_mfx_window_ref (GstMfxWindow * window)
{
  return gst_mfx_window_ref_internal (window);
}

/**
 * gst_mfx_window_unref:
 * @window: a #GstMfxWindow
 *
 * Atomically decreases the reference count of the @window by one. If
 * the reference count reaches zero, the window will be free'd.
 */
void
gst_mfx_window_unref (GstMfxWindow * window)
{
  gst_mfx_window_unref_internal (window);
}

/**
 * gst_mfx_window_replace:
 * @old_window_ptr: a pointer to a #GstMfxWindow
 * @new_window: a #GstMfxWindow
 *
 * Atomically replaces the window window held in @old_window_ptr with
 * @new_window. This means that @old_window_ptr shall reference a
 * valid window. However, @new_window can be NULL.
 */
void
gst_mfx_window_replace (GstMfxWindow ** old_window_ptr,
    GstMfxWindow * new_window)
{
  gst_mfx_window_replace_internal (old_window_ptr, new_window);
}

/**
 * gst_mfx_window_get_display:
 * @window: a #GstMfxWindow
 *
 * Returns the #GstMfxDisplay this @window is bound to.
 *
 * Return value: the parent #GstMfxDisplay object
 */
GstMfxDisplay *
gst_mfx_window_get_display (GstMfxWindow * window)
{
  g_return_val_if_fail (window != NULL, NULL);

  return GST_MFX_OBJECT_DISPLAY (window);
}

/**
 * gst_mfx_window_show:
 * @window: a #GstMfxWindow
 *
 * Flags a window to be displayed. Any window that is not shown will
 * not appear on the screen.
 */
void
gst_mfx_window_show (GstMfxWindow * window)
{
  g_return_if_fail (window != NULL);

  GST_MFX_WINDOW_GET_CLASS (window)->show (window);
  window->check_geometry = TRUE;
}

/**
 * gst_mfx_window_hide:
 * @window: a #GstMfxWindow
 *
 * Reverses the effects of gst_mfx_window_show (), causing the window
 * to be hidden (invisible to the user).
 */
void
gst_mfx_window_hide (GstMfxWindow * window)
{
  g_return_if_fail (window != NULL);

  GST_MFX_WINDOW_GET_CLASS (window)->hide (window);
}

/**
 * gst_mfx_window_get_fullscreen:
 * @window: a #GstMfxWindow
 *
 * Retrieves whether the @window is fullscreen or not
 *
 * Return value: %TRUE if the window is fullscreen
 */
gboolean
gst_mfx_window_get_fullscreen (GstMfxWindow * window)
{
  g_return_val_if_fail (window != NULL, FALSE);

  gst_mfx_window_ensure_size (window);

  return window->is_fullscreen;
}

/**
 * gst_mfx_window_set_fullscreen:
 * @window: a #GstMfxWindow
 * @fullscreen: %TRUE to request window to get fullscreen
 *
 * Requests to place the @window in fullscreen or unfullscreen states.
 */
void
gst_mfx_window_set_fullscreen (GstMfxWindow * window, gboolean fullscreen)
{
  const GstMfxWindowClass *klass;

  g_return_if_fail (window != NULL);

  klass = GST_MFX_WINDOW_GET_CLASS (window);

  if (window->is_fullscreen != fullscreen &&
      klass->set_fullscreen && klass->set_fullscreen (window, fullscreen)) {
    window->is_fullscreen = fullscreen;
    window->check_geometry = TRUE;
  }
}

/**
 * gst_mfx_window_get_width:
 * @window: a #GstMfxWindow
 *
 * Retrieves the width of a #GstMfxWindow.
 *
 * Return value: the width of the @window, in pixels
 */
guint
gst_mfx_window_get_width (GstMfxWindow * window)
{
  g_return_val_if_fail (window != NULL, 0);

  gst_mfx_window_ensure_size (window);

  return window->width;
}

/**
 * gst_mfx_window_get_height:
 * @window: a #GstMfxWindow
 *
 * Retrieves the height of a #GstMfxWindow
 *
 * Return value: the height of the @window, in pixels
 */
guint
gst_mfx_window_get_height (GstMfxWindow * window)
{
  g_return_val_if_fail (window != NULL, 0);

  gst_mfx_window_ensure_size (window);

  return window->height;
}

/**
 * gst_mfx_window_get_size:
 * @window: a #GstMfxWindow
 * @width_ptr: return location for the width, or %NULL
 * @height_ptr: return location for the height, or %NULL
 *
 * Retrieves the dimensions of a #GstMfxWindow.
 */
void
gst_mfx_window_get_size (GstMfxWindow * window, guint * width_ptr,
    guint * height_ptr)
{
  g_return_if_fail (window != NULL);

  gst_mfx_window_ensure_size (window);

  if (width_ptr)
    *width_ptr = window->width;

  if (height_ptr)
    *height_ptr = window->height;
}

/**
 * gst_mfx_window_set_width:
 * @window: a #GstMfxWindow
 * @width: requested new width for the window, in pixels
 *
 * Resizes the @window to match the specified @width.
 */
void
gst_mfx_window_set_width (GstMfxWindow * window, guint width)
{
  g_return_if_fail (window != NULL);

  gst_mfx_window_set_size (window, width, window->height);
}

/**
 * gst_mfx_window_set_height:
 * @window: a #GstMfxWindow
 * @height: requested new height for the window, in pixels
 *
 * Resizes the @window to match the specified @height.
 */
void
gst_mfx_window_set_height (GstMfxWindow * window, guint height)
{
  g_return_if_fail (window != NULL);

  gst_mfx_window_set_size (window, window->width, height);
}

/**
 * gst_mfx_window_set_size:
 * @window: a #GstMfxWindow
 * @width: requested new width for the window, in pixels
 * @height: requested new height for the window, in pixels
 *
 * Resizes the @window to match the specified @width and @height.
 */
void
gst_mfx_window_set_size (GstMfxWindow * window, guint width, guint height)
{
  g_return_if_fail (window != NULL);

  if (width == window->width && height == window->height)
    return;

  if (!GST_MFX_WINDOW_GET_CLASS (window)->resize (window, width, height))
    return;

  window->width = width;
  window->height = height;
}

static inline void
get_surface_rect (GstMfxSurfaceProxy * proxy, GstMfxRectangle * rect)
{
  rect->x = 0;
  rect->y = 0;
  rect->width = GST_MFX_SURFACE_PROXY_WIDTH (proxy);
  rect->height = GST_MFX_SURFACE_PROXY_HEIGHT (proxy);
}

static inline void
get_window_rect (GstMfxWindow * window, GstMfxRectangle * rect)
{
  guint width, height;

  gst_mfx_window_get_size (window, &width, &height);
  rect->x = 0;
  rect->y = 0;
  rect->width = width;
  rect->height = height;
}

/**
 * gst_mfx_window_put_surface:
 * @window: a #GstMfxWindow
 * @surface: a #GstMfxSurfaceProxy
 * @src_rect: the sub-rectangle of the source surface to
 *   extract and process. If %NULL, the entire surface will be used.
 * @dst_rect: the sub-rectangle of the destination
 *   window into which the surface is rendered. If %NULL, the entire
 *   window will be used.
 *
 * Renders the @surface region specified by @src_rect into the @window
 * region specified by @dst_rect.
 *
 * Return value: %TRUE on success
 */
gboolean
gst_mfx_window_put_surface (GstMfxWindow * window,
    GstMfxSurfaceProxy * proxy,
    const GstMfxRectangle * src_rect, const GstMfxRectangle * dst_rect)
{
  const GstMfxWindowClass *klass;
  GstMfxRectangle src_rect_default, dst_rect_default;

  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (proxy != NULL, FALSE);

  klass = GST_MFX_WINDOW_GET_CLASS (window);
  if (!klass->render)
    return FALSE;

  if (!src_rect) {
    src_rect = &src_rect_default;
    get_surface_rect (proxy, &src_rect_default);
  }

  if (!dst_rect) {
    dst_rect = &dst_rect_default;
    get_window_rect (window, &dst_rect_default);
  }

  return klass->render (window, proxy, src_rect, dst_rect);
}

/**
 * gst_mfx_window_reconfigure:
 * @window: a #GstMfxWindow
 *
 * Updates internal window size from geometry of the underlying window
 * implementation if necessary.
 */
void
gst_mfx_window_reconfigure (GstMfxWindow * window)
{
  g_return_if_fail (window != NULL);

  window->check_geometry = TRUE;
  gst_mfx_window_ensure_size (window);
}

/**
 * gst_mfx_window_unblock:
 * @window: a #GstMfxWindow
 *
 * Unblocks a rendering surface operation.
 */
gboolean
gst_mfx_window_unblock (GstMfxWindow * window)
{
  const GstMfxWindowClass *klass;

  g_return_val_if_fail (window != NULL, FALSE);

  klass = GST_MFX_WINDOW_GET_CLASS (window);

  if (klass->unblock)
    return klass->unblock (window);

  return TRUE;
}

/**
 * gst_mfx_window_unblock_cancel:
 * @window: a #GstMfxWindow
 *
 * Cancels the previous unblock request.
 */
gboolean
gst_mfx_window_unblock_cancel (GstMfxWindow * window)
{
  const GstMfxWindowClass *klass;

  g_return_val_if_fail (window != NULL, FALSE);

  klass = GST_MFX_WINDOW_GET_CLASS (window);

  if (klass->unblock_cancel)
    return klass->unblock_cancel (window);

  return TRUE;
}
