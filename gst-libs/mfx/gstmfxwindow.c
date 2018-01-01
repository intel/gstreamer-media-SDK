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
#include "gstmfxcontext.h"
#include "gstmfxsurface.h"

#define DEBUG 1
#include "gstmfxdebug.h"

G_DEFINE_TYPE_WITH_PRIVATE (GstMfxWindow, gst_mfx_window, GST_TYPE_OBJECT);

static void
gst_mfx_window_init (GstMfxWindow * window)
{
  GstMfxWindowPrivate *const priv =
      gst_mfx_window_get_instance_private (window);

  window->priv = priv;
}

static void
gst_mfx_window_ensure_size (GstMfxWindow * window)
{
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE (window);
  const GstMfxWindowClass *const klass = GST_MFX_WINDOW_GET_CLASS (window);
  guint width = priv->width, height = priv->height;

  if (!priv->check_geometry)
    return;

  if (klass->get_geometry)
    klass->get_geometry (window, NULL, NULL, &priv->width, &priv->height);

  priv->check_geometry = FALSE;

  if (width == priv->width && height == priv->height)
    return;

  if (klass->resize)
    klass->resize (window, priv->width, priv->height);
}

static gboolean
gst_mfx_window_create (GstMfxWindow * window, GstMfxID id, guint width,
    guint height)
{
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE (window);

  if (!GST_MFX_WINDOW_GET_CLASS (window)->create (window, &width, &height))
    return FALSE;

  if (width != priv->width || height != priv->height) {
    GST_DEBUG ("backend resized window to %ux%u", width, height);
    priv->width = width;
    priv->height = height;
  }

  return TRUE;
}

static void
gst_mfx_window_finalize (GObject * object)
{
  GstMfxWindowClass *klass = GST_MFX_WINDOW_GET_CLASS (object);
  GstMfxWindow *window = GST_MFX_WINDOW (object);
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE (window);

  if (klass->destroy)
    klass->destroy (object);

  gst_mfx_context_replace (&priv->context, NULL);

  G_OBJECT_CLASS (gst_mfx_window_parent_class)->finalize (object);
}

static void
gst_mfx_window_class_init (GstMfxWindowClass * klass)
{
  GObjectClass *const object_class = G_OBJECT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_debug_mfx, "mfx", 0, "MFX helper");

  object_class->finalize = gst_mfx_window_finalize;
}

GstMfxWindow *
gst_mfx_window_new_internal (GstMfxWindow * window, GstMfxContext * context,
    GstMfxID id, guint width, guint height)
{
  if (id != GST_MFX_ID_INVALID) {
    g_return_val_if_fail (width == 0, NULL);
    g_return_val_if_fail (height == 0, NULL);
  } else {
    g_return_val_if_fail (width > 0, NULL);
    g_return_val_if_fail (height > 0, NULL);
  }

  GST_MFX_WINDOW_GET_PRIVATE (window)->handle = id;
  GST_MFX_WINDOW_GET_PRIVATE (window)->use_foreign_window =
      id != GST_MFX_ID_INVALID;
  GST_MFX_WINDOW_GET_PRIVATE (window)->context = context ?
      gst_mfx_context_ref (context) : NULL;
  if (!gst_mfx_window_create (window, id, width, height))
    goto error;
  return window;

error:
  gst_mfx_window_unref (window);
  return NULL;
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
  g_return_val_if_fail (window != NULL, NULL);

  return gst_object_ref (GST_OBJECT (window));
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
  gst_object_unref (GST_OBJECT (window));
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
  g_return_if_fail (old_window_ptr != NULL);

  gst_object_replace ((GstObject **) old_window_ptr, GST_OBJECT (new_window));
}

GstMfxContext *
gst_mfx_window_get_context (GstMfxWindow * window)
{
  g_return_val_if_fail (window != NULL, NULL);

  return GST_MFX_WINDOW_GET_PRIVATE (window)->context;
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
  GST_MFX_WINDOW_GET_PRIVATE (window)->check_geometry = TRUE;
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

guintptr
gst_mfx_window_get_handle (GstMfxWindow * window)
{
  g_return_val_if_fail (window != NULL, (guintptr) NULL);

  return GST_MFX_WINDOW_GET_PRIVATE (window)->handle;
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

  return GST_MFX_WINDOW_GET_PRIVATE (window)->is_fullscreen;
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
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE (window);
  const GstMfxWindowClass *klass = GST_MFX_WINDOW_GET_CLASS (window);

  g_return_if_fail (window != NULL);

  if (priv->is_fullscreen != fullscreen &&
      klass->set_fullscreen && klass->set_fullscreen (window, fullscreen)) {
    priv->is_fullscreen = fullscreen;
    priv->check_geometry = TRUE;
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

  return GST_MFX_WINDOW_GET_PRIVATE (window)->width;
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

  return GST_MFX_WINDOW_GET_PRIVATE (window)->height;
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
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE (window);

  g_return_if_fail (window != NULL);

  gst_mfx_window_ensure_size (window);

  if (width_ptr)
    *width_ptr = priv->width;

  if (height_ptr)
    *height_ptr = priv->height;
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

  gst_mfx_window_set_size (window, width,
      GST_MFX_WINDOW_GET_PRIVATE (window)->height);
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

  gst_mfx_window_set_size (window, GST_MFX_WINDOW_GET_PRIVATE (window)->width,
      height);
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
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE (window);

  g_return_if_fail (window != NULL);

  if (width == priv->width && height == priv->height)
    return;

  if (!GST_MFX_WINDOW_GET_CLASS (window)->resize (window, width, height))
    return;

  priv->width = width;
  priv->height = height;
}

static inline void
get_surface_rect (GstMfxSurface * surface, GstMfxRectangle * rect)
{
  rect->x = 0;
  rect->y = 0;
  rect->width = GST_MFX_SURFACE_WIDTH (surface);
  rect->height = GST_MFX_SURFACE_HEIGHT (surface);
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
 * @surface: a #GstMfxSurface
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
    GstMfxSurface * surface,
    const GstMfxRectangle * src_rect, const GstMfxRectangle * dst_rect)
{
  const GstMfxWindowClass *klass;
  GstMfxRectangle src_rect_default, dst_rect_default;

  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (surface != NULL, FALSE);

  klass = GST_MFX_WINDOW_GET_CLASS (window);
  if (!klass->render)
    return FALSE;

  if (!src_rect) {
    src_rect = &src_rect_default;
    get_surface_rect (surface, &src_rect_default);
  }

  if (!dst_rect) {
    dst_rect = &dst_rect_default;
    get_window_rect (window, &dst_rect_default);
  }

  return klass->render (window, surface, src_rect, dst_rect);
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

  GST_MFX_WINDOW_GET_PRIVATE (window)->check_geometry = TRUE;
  gst_mfx_window_ensure_size (window);
}
