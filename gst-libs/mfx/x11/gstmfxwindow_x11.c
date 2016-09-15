/*
 *  Copyright (C) 2012-2014 Intel Corporation
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
#include <string.h>
#include <X11/Xatom.h>
#include <X11/Xlib-xcb.h>

#ifdef HAVE_XCBDRI3
#include <xcb/dri3.h>
#endif

#ifdef HAVE_XCBPRESENT
#include <xcb/present.h>
#endif

#include <X11/Xlib.h>

#include "gstmfxwindow_x11.h"
#include "gstmfxwindow_x11_priv.h"
#include "gstmfxdisplay_x11.h"
#include "gstmfxdisplay_x11_priv.h"
#include "gstmfxutils_vaapi.h"
#include "gstmfxutils_x11.h"
#include "gstmfxprimebufferproxy.h"

#define DEBUG 1
#include "gstmfxdebug.h"

#define _NET_WM_STATE_REMOVE    0       /* remove/unset property */
#define _NET_WM_STATE_ADD       1       /* add/set property      */
#define _NET_WM_STATE_TOGGLE    2       /* toggle property       */

static void
send_wmspec_change_state (GstMfxWindow * window, Atom state, gboolean add)
{
  GstMfxWindowX11Private *const priv = GST_MFX_WINDOW_X11_GET_PRIVATE (window);
  Display *const dpy = GST_MFX_OBJECT_NATIVE_DISPLAY (window);
  XClientMessageEvent xclient;

  memset (&xclient, 0, sizeof (xclient));

  xclient.type = ClientMessage;
  xclient.window = GST_MFX_OBJECT_ID (window);
  xclient.message_type = priv->atom_NET_WM_STATE;
  xclient.format = 32;

  xclient.data.l[0] = add ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
  xclient.data.l[1] = state;
  xclient.data.l[2] = 0;
  xclient.data.l[3] = 0;
  xclient.data.l[4] = 0;

  XSendEvent (dpy,
      DefaultRootWindow (dpy),
      False,
      SubstructureRedirectMask | SubstructureNotifyMask, (XEvent *) & xclient);
}

static void
wait_event (GstMfxWindow * window, int type)
{
  Display *const dpy = GST_MFX_OBJECT_NATIVE_DISPLAY (window);
  const Window xid = GST_MFX_OBJECT_ID (window);
  XEvent e;
  Bool got_event;

  for (;;) {
    GST_MFX_OBJECT_LOCK_DISPLAY (window);
    got_event = XCheckTypedWindowEvent (dpy, xid, type, &e);
    GST_MFX_OBJECT_UNLOCK_DISPLAY (window);
    if (got_event)
      break;
    g_usleep (10);
  }
}

static gboolean
timed_wait_event (GstMfxWindow * window, int type, guint64 end_time, XEvent * e)
{
  Display *const dpy = GST_MFX_OBJECT_NATIVE_DISPLAY (window);
  const Window xid = GST_MFX_OBJECT_ID (window);
  XEvent tmp_event;
  GTimeVal now;
  guint64 now_time;
  Bool got_event;

  if (!e)
    e = &tmp_event;

  GST_MFX_OBJECT_LOCK_DISPLAY (window);
  got_event = XCheckTypedWindowEvent (dpy, xid, type, e);
  GST_MFX_OBJECT_UNLOCK_DISPLAY (window);
  if (got_event)
    return TRUE;

  do {
    g_usleep (10);
    GST_MFX_OBJECT_LOCK_DISPLAY (window);
    got_event = XCheckTypedWindowEvent (dpy, xid, type, e);
    GST_MFX_OBJECT_UNLOCK_DISPLAY (window);
    if (got_event)
      return TRUE;
    g_get_current_time (&now);
    now_time = (guint64) now.tv_sec * 1000000 + now.tv_usec;
  } while (now_time < end_time);
  return FALSE;
}

static gboolean
gst_mfx_window_x11_show (GstMfxWindow * window)
{
  GstMfxWindowX11Private *const priv = GST_MFX_WINDOW_X11_GET_PRIVATE (window);
  Display *const dpy = GST_MFX_OBJECT_NATIVE_DISPLAY (window);
  const Window xid = GST_MFX_OBJECT_ID (window);
  XWindowAttributes wattr;
  gboolean has_errors;

  if (priv->is_mapped)
    return TRUE;

  GST_MFX_OBJECT_LOCK_DISPLAY (window);
  x11_trap_errors ();
  if (window->use_foreign_window) {
    XGetWindowAttributes (dpy, xid, &wattr);
    if (!(wattr.your_event_mask & StructureNotifyMask))
      XSelectInput (dpy, xid, StructureNotifyMask);
  }
  XMapWindow (dpy, xid);
  has_errors = x11_untrap_errors () != 0;
  GST_MFX_OBJECT_UNLOCK_DISPLAY (window);

  if (!has_errors) {
    wait_event (window, MapNotify);
    if (window->use_foreign_window &&
        !(wattr.your_event_mask & StructureNotifyMask)) {
      GST_MFX_OBJECT_LOCK_DISPLAY (window);
      x11_trap_errors ();
      XSelectInput (dpy, xid, wattr.your_event_mask);
      has_errors = x11_untrap_errors () != 0;
      GST_MFX_OBJECT_UNLOCK_DISPLAY (window);
    }
    priv->is_mapped = TRUE;

    if (priv->fullscreen_on_map)
      gst_mfx_window_set_fullscreen (window, TRUE);
  }
  return !has_errors;
}

static gboolean
gst_mfx_window_x11_hide (GstMfxWindow * window)
{
  GstMfxWindowX11Private *const priv = GST_MFX_WINDOW_X11_GET_PRIVATE (window);
  Display *const dpy = GST_MFX_OBJECT_NATIVE_DISPLAY (window);
  const Window xid = GST_MFX_OBJECT_ID (window);
  XWindowAttributes wattr;
  gboolean has_errors;

  if (!priv->is_mapped)
    return TRUE;

  GST_MFX_OBJECT_LOCK_DISPLAY (window);
  x11_trap_errors ();
  if (window->use_foreign_window) {
    XGetWindowAttributes (dpy, xid, &wattr);
    if (!(wattr.your_event_mask & StructureNotifyMask))
      XSelectInput (dpy, xid, StructureNotifyMask);
  }
  XUnmapWindow (dpy, xid);
  has_errors = x11_untrap_errors () != 0;
  GST_MFX_OBJECT_UNLOCK_DISPLAY (window);

  if (!has_errors) {
    wait_event (window, UnmapNotify);
    if (window->use_foreign_window &&
        !(wattr.your_event_mask & StructureNotifyMask)) {
      GST_MFX_OBJECT_LOCK_DISPLAY (window);
      x11_trap_errors ();
      XSelectInput (dpy, xid, wattr.your_event_mask);
      has_errors = x11_untrap_errors () != 0;
      GST_MFX_OBJECT_UNLOCK_DISPLAY (window);
    }
    priv->is_mapped = FALSE;
  }
  return !has_errors;
}

static gboolean
gst_mfx_window_x11_create (GstMfxWindow * window, guint * width, guint * height)
{
  GstMfxWindowX11Private *const priv = GST_MFX_WINDOW_X11_GET_PRIVATE (window);
  GstMfxDisplay *const display = GST_MFX_OBJECT_DISPLAY (window);
  Display *const dpy = GST_MFX_OBJECT_NATIVE_DISPLAY (window);
  Window xid = GST_MFX_OBJECT_ID (window);
  guint vid = 0;
  Colormap cmap = None;
  const GstMfxDisplayClass *display_class;
  const GstMfxWindowClass *window_class;
  XWindowAttributes wattr;
  Atom atoms[2];
  gboolean ok;

  static const char *atom_names[2] = {
    "_NET_WM_STATE",
    "_NET_WM_STATE_FULLSCREEN",
  };

  if (window->use_foreign_window && xid) {
    GST_MFX_OBJECT_LOCK_DISPLAY (window);
    XGetWindowAttributes (dpy, xid, &wattr);
    priv->is_mapped = wattr.map_state == IsViewable;
    ok = x11_get_geometry (dpy, xid, NULL, NULL, width, height, NULL);
    GST_MFX_OBJECT_UNLOCK_DISPLAY (window);
    return ok;
  }

  display_class = GST_MFX_DISPLAY_GET_CLASS (display);
  if (display_class) {
    if (display_class->get_visual_id)
      vid = display_class->get_visual_id (display, window);
    if (display_class->get_colormap)
      cmap = display_class->get_colormap (display, window);
  }

  window_class = GST_MFX_WINDOW_GET_CLASS (window);
  if (window_class) {
    if (window_class->get_visual_id && !vid)
      vid = window_class->get_visual_id (window);
    if (window_class->get_colormap && !cmap)
      cmap = window_class->get_colormap (window);
  }

  GST_MFX_OBJECT_LOCK_DISPLAY (window);
  XInternAtoms (dpy,
      (char **) atom_names, G_N_ELEMENTS (atom_names), False, atoms);
  priv->atom_NET_WM_STATE = atoms[0];
  priv->atom_NET_WM_STATE_FULLSCREEN = atoms[1];

  xid = x11_create_window (dpy, *width, *height, vid, cmap);
  if (xid)
    XRaiseWindow (dpy, xid);
  GST_MFX_OBJECT_UNLOCK_DISPLAY (window);

  GST_MFX_OBJECT_ID (window) = xid;
  return xid != None;
}

static void
gst_mfx_window_x11_destroy (GstMfxWindow * window)
{
  Display *const dpy = GST_MFX_OBJECT_NATIVE_DISPLAY (window);
  const Window xid = GST_MFX_OBJECT_ID (window);

  if (xid) {
    GST_MFX_OBJECT_LOCK_DISPLAY (window);
    XDestroyWindow (dpy, xid);
    GST_MFX_OBJECT_UNLOCK_DISPLAY (window);

    GST_MFX_OBJECT_ID (window) = None;
  }
}

static gboolean
gst_mfx_window_x11_get_geometry (GstMfxWindow * window,
    gint * px, gint * py, guint * pwidth, guint * pheight)
{
  Display *const dpy = GST_MFX_OBJECT_NATIVE_DISPLAY (window);
  const Window xid = GST_MFX_OBJECT_ID (window);
  gboolean success;

  GST_MFX_OBJECT_LOCK_DISPLAY (window);
  success = x11_get_geometry (dpy, xid, px, py, pwidth, pheight, NULL);
  GST_MFX_OBJECT_UNLOCK_DISPLAY (window);
  return success;
}

static gboolean
gst_mfx_window_x11_set_fullscreen (GstMfxWindow * window, gboolean fullscreen)
{
  GstMfxWindowX11Private *const priv = GST_MFX_WINDOW_X11_GET_PRIVATE (window);
  Display *const dpy = GST_MFX_OBJECT_NATIVE_DISPLAY (window);
  const Window xid = GST_MFX_OBJECT_ID (window);
  XEvent e;
  guint width, height;
  gboolean has_errors;
  GTimeVal now;
  guint64 end_time;

  GST_MFX_OBJECT_LOCK_DISPLAY (window);
  x11_trap_errors ();
  if (fullscreen) {
    if (!priv->is_mapped) {
      priv->fullscreen_on_map = TRUE;

      XChangeProperty (dpy,
          xid,
          priv->atom_NET_WM_STATE, XA_ATOM, 32,
          PropModeReplace,
          (unsigned char *) &priv->atom_NET_WM_STATE_FULLSCREEN, 1);
    } else {
      send_wmspec_change_state (window,
          priv->atom_NET_WM_STATE_FULLSCREEN, TRUE);
    }
  } else {
    if (!priv->is_mapped) {
      priv->fullscreen_on_map = FALSE;

      XDeleteProperty (dpy, xid, priv->atom_NET_WM_STATE);
    } else {
      send_wmspec_change_state (window,
          priv->atom_NET_WM_STATE_FULLSCREEN, FALSE);
    }
  }
  XSync (dpy, False);
  has_errors = x11_untrap_errors () != 0;
  GST_MFX_OBJECT_UNLOCK_DISPLAY (window);
  if (has_errors)
    return FALSE;

  /* Try to wait for the completion of the fullscreen mode switch */
  if (priv->is_mapped) {
    const guint DELAY = 100000; /* 100 ms */
    g_get_current_time (&now);
    end_time = DELAY + ((guint64) now.tv_sec * 1000000 + now.tv_usec);
    while (timed_wait_event (window, ConfigureNotify, end_time, &e)) {
      if (fullscreen) {
        gst_mfx_display_get_size (GST_MFX_OBJECT_DISPLAY (window),
            &width, &height);
        if (e.xconfigure.width == width && e.xconfigure.height == height)
          return TRUE;
      } else {
        gst_mfx_window_get_size (window, &width, &height);
        if (e.xconfigure.width != width || e.xconfigure.height != height)
          return TRUE;
      }
    }
  }
  return FALSE;
}

static gboolean
gst_mfx_window_x11_resize (GstMfxWindow * window, guint width, guint height)
{
  gboolean has_errors;

  if (!GST_MFX_OBJECT_ID (window))
    return FALSE;

  GST_MFX_OBJECT_LOCK_DISPLAY (window);
  x11_trap_errors ();
  XResizeWindow (GST_MFX_OBJECT_NATIVE_DISPLAY (window),
      GST_MFX_OBJECT_ID (window), width, height);
  has_errors = x11_untrap_errors () != 0;
  GST_MFX_OBJECT_UNLOCK_DISPLAY (window);
  return !has_errors;
}

static gboolean
gst_mfx_window_x11_render (GstMfxWindow * window,
    GstMfxSurfaceProxy * proxy,
    const GstMfxRectangle * src_rect, const GstMfxRectangle * dst_rect)
{
#if defined(HAVE_XCBDRI3) && defined(HAVE_XCBPRESENT)
  GstMfxDisplayX11 *const x11_display =
        GST_MFX_DISPLAY_X11 (GST_MFX_OBJECT_DISPLAY (window));
  GstMfxPrimeBufferProxy *buffer_proxy;

  Display *display = gst_mfx_display_x11_get_display (x11_display);
  int fd = 0, x = 0, y = 0, bpp = 0;
  xcb_connection_t *xcbconn;
  unsigned int crop_w, crop_h, width, height, border, depth, stride, size;
  Window root;
  xcb_pixmap_t pixmap;

  buffer_proxy = gst_mfx_prime_buffer_proxy_new_from_surface (proxy);
  if (!buffer_proxy)
      return FALSE;

  fd = GST_MFX_PRIME_BUFFER_PROXY_HANDLE (buffer_proxy);

  xcbconn = XGetXCBConnection (display);

  crop_w = src_rect->width - src_rect->x;
  crop_h = src_rect->height - src_rect->y;

  XResizeWindow (display, GST_MFX_OBJECT_ID (window), crop_w, crop_h);
  XGetGeometry (display, GST_MFX_OBJECT_ID (window), &root, &x, &y,
      &width, &height, &border, &depth);

  switch (depth) {
    case 8:
      bpp = 8;
      break;
    case 15:
    case 16:
      bpp = 16;
      break;
    case 24:
    case 32:
      bpp = 32;
      break;
    default:
      break;
  }

  width = src_rect->width;
  height = src_rect->height;
  stride = width * bpp / 8;
  size = GST_ROUND_UP_N (stride * height, 4096);

  pixmap = xcb_generate_id (xcbconn);
  xcb_dri3_pixmap_from_buffer (xcbconn, pixmap, root, size,
      width, height, stride, depth, bpp, fd);
  if (!pixmap)
    return FALSE;

  GST_MFX_OBJECT_LOCK_DISPLAY (window);
  xcb_present_pixmap (xcbconn, GST_MFX_OBJECT_ID (window), pixmap,
      0, 0, 0, 0, 0, None, None, None, XCB_PRESENT_OPTION_NONE, 0, 0, 0, 0,
      NULL);
  GST_MFX_OBJECT_UNLOCK_DISPLAY (window);

  xcb_free_pixmap (xcbconn, pixmap);
  xcb_flush (xcbconn);
#else
  GST_ERROR("Unable to render the video.\n");
#endif
  return TRUE;
}

void
gst_mfx_window_x11_class_init (GstMfxWindowX11Class * klass)
{
  GstMfxObjectClass *const object_class = GST_MFX_OBJECT_CLASS (klass);
  GstMfxWindowClass *const window_class = GST_MFX_WINDOW_CLASS (klass);

  object_class->finalize = (GstMfxObjectFinalizeFunc)
      gst_mfx_window_x11_destroy;

  window_class->create = gst_mfx_window_x11_create;
  window_class->show = gst_mfx_window_x11_show;
  window_class->hide = gst_mfx_window_x11_hide;
  window_class->get_geometry = gst_mfx_window_x11_get_geometry;
  window_class->set_fullscreen = gst_mfx_window_x11_set_fullscreen;
  window_class->resize = gst_mfx_window_x11_resize;
  window_class->render = gst_mfx_window_x11_render;
}

#define gst_mfx_window_x11_finalize gst_mfx_window_x11_destroy

GST_MFX_OBJECT_DEFINE_CLASS_WITH_CODE (GstMfxWindowX11,
    gst_mfx_window_x11, gst_mfx_window_x11_class_init (&g_class));


/**
 * gst_mfx_window_x11_new:
 * @display: a #GstMfxDisplay
 * @width: the requested window width, in pixels
 * @height: the requested windo height, in pixels
 *
 * Creates a window with the specified @width and @height. The window
 * will be attached to the @display and remains invisible to the user
 * until gst_mfx_window_show() is called.
 *
 * Return value: the newly allocated #GstMfxWindow object
 */
GstMfxWindow *
gst_mfx_window_x11_new (GstMfxDisplay * display, guint width, guint height)
{
  GST_DEBUG ("new window, size %ux%u", width, height);

  g_return_val_if_fail (GST_MFX_IS_DISPLAY_X11 (display), NULL);

  return
      gst_mfx_window_new_internal (gst_mfx_window_x11_class (),
          display, GST_MFX_ID_INVALID, width, height);
}

/**
 * gst_mfx_window_x11_new_with_xid:
 * @display: a #GstMfxDisplay
 * @xid: an X11 #Window id
 *
 * Creates a #GstMfxWindow using the X11 #Window @xid. The caller
 * still owns the window and must call XDestroyWindow() when all
 * #GstMfxWindow references are released. Doing so too early can
 * yield undefined behaviour.
 *
 * Return value: the newly allocated #GstMfxWindow object
 */
GstMfxWindow *
gst_mfx_window_x11_new_with_xid (GstMfxDisplay * display, Window xid)
{
  GST_DEBUG ("new window from xid 0x%08x", (guint) xid);

  g_return_val_if_fail (GST_MFX_IS_DISPLAY_X11 (display), NULL);
  g_return_val_if_fail (xid != None, NULL);

  return gst_mfx_window_new_internal (GST_MFX_WINDOW_CLASS
      (gst_mfx_window_x11_class ()), display, xid, 0, 0);
}

/**
 * gst_mfx_window_x11_get_xid:
 * @window: a #GstMfxWindowX11
 *
 * Returns the underlying X11 #Window that was created by
 * gst_mfx_window_x11_new()
 *
 * Return value: the underlying X11 #Window bound to @window.
 */
Window
gst_mfx_window_x11_get_xid (GstMfxWindowX11 * window)
{
  g_return_val_if_fail (window != NULL, None);

  return GST_MFX_OBJECT_ID (window);
}
