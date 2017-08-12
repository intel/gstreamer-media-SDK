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

#ifdef HAVE_XRENDER
#include <X11/extensions/Xrender.h>
#endif

#ifdef HAVE_XCBDRI3
#include <xcb/dri3.h>
#endif

#ifdef HAVE_XCBPRESENT
# include <xcb/present.h>
#endif

#include <X11/Xlib.h>

#include "gstmfxdisplay_x11.h"
#include "gstmfxdisplay_x11_priv.h"
#include "gstmfxwindow_x11.h"
#include "gstmfxwindow_x11_priv.h"
#include "gstmfxutils_vaapi.h"
#include "gstmfxutils_x11.h"
#include "gstmfxprimebufferproxy.h"
#include "gstmfxsurface_vaapi.h"

#define DEBUG 1
#include "gstmfxdebug.h"

#define _NET_WM_STATE_REMOVE    0       /* remove/unset property */
#define _NET_WM_STATE_ADD       1       /* add/set property      */
#define _NET_WM_STATE_TOGGLE    2       /* toggle property       */

static void
send_wmspec_change_state (GstMfxWindow * window, Atom state, gboolean add)
{
  GstMfxWindowX11Private *const priv = GST_MFX_WINDOW_X11_GET_PRIVATE (window);
  Display *const dpy = GST_MFX_DISPLAY_HANDLE (priv->display);
  XClientMessageEvent xclient;

  memset (&xclient, 0, sizeof (xclient));

  xclient.type = ClientMessage;
  xclient.window = GST_MFX_WINDOW_ID (window);
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
  GstMfxWindowX11Private *const priv = GST_MFX_WINDOW_X11_GET_PRIVATE (window);
  Display *const dpy = GST_MFX_DISPLAY_HANDLE (priv->display);
  const Window xid = GST_MFX_WINDOW_ID (window);
  XEvent e;
  Bool got_event;

  for (;;) {
    GST_MFX_DISPLAY_LOCK (priv->display);
    got_event = XCheckTypedWindowEvent (dpy, xid, type, &e);
    GST_MFX_DISPLAY_UNLOCK (priv->display);
    if (got_event)
      break;
    g_usleep (10);
  }
}

static gboolean
timed_wait_event (GstMfxWindow * window, int type, guint64 end_time, XEvent * e)
{
  GstMfxWindowX11Private *const priv = GST_MFX_WINDOW_X11_GET_PRIVATE (window);
  Display *const dpy = GST_MFX_DISPLAY_HANDLE (priv->display);
  const Window xid = GST_MFX_WINDOW_ID (window);
  XEvent tmp_event;
  GTimeVal now;
  guint64 now_time;
  Bool got_event;

  if (!e)
    e = &tmp_event;

  GST_MFX_DISPLAY_LOCK (priv->display);
  got_event = XCheckTypedWindowEvent (dpy, xid, type, e);
  GST_MFX_DISPLAY_UNLOCK (priv->display);
  if (got_event)
    return TRUE;

  do {
    g_usleep (10);
    GST_MFX_DISPLAY_LOCK (priv->display);
    got_event = XCheckTypedWindowEvent (dpy, xid, type, e);
    GST_MFX_DISPLAY_UNLOCK (priv->display);
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
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE (window);
  GstMfxWindowX11Private *const priv2 = GST_MFX_WINDOW_X11_GET_PRIVATE (window);
  Display *const dpy = GST_MFX_DISPLAY_HANDLE (priv2->display);
  const Window xid = GST_MFX_WINDOW_ID (window);
  XWindowAttributes wattr;
  gboolean has_errors;

  if (priv2->is_mapped)
    return TRUE;

  GST_MFX_DISPLAY_LOCK (priv2->display);
  x11_trap_errors ();
  if (priv->use_foreign_window) {
    XGetWindowAttributes (dpy, xid, &wattr);
    if (!(wattr.your_event_mask & StructureNotifyMask))
      XSelectInput (dpy, xid, StructureNotifyMask);
  }
  XMapWindow (dpy, xid);
  has_errors = x11_untrap_errors () != 0;
  GST_MFX_DISPLAY_UNLOCK (priv2->display);

  if (!has_errors) {
    wait_event (window, MapNotify);
    if (priv->use_foreign_window &&
        !(wattr.your_event_mask & StructureNotifyMask)) {
      GST_MFX_DISPLAY_LOCK (priv2->display);
      x11_trap_errors ();
      XSelectInput (dpy, xid, wattr.your_event_mask);
      has_errors = x11_untrap_errors () != 0;
      GST_MFX_DISPLAY_UNLOCK (priv2->display);
    }
    priv2->is_mapped = TRUE;

    if (priv2->fullscreen_on_map)
      gst_mfx_window_set_fullscreen (window, TRUE);
  }

  return !has_errors;
}

static gboolean
gst_mfx_window_x11_hide (GstMfxWindow * window)
{
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE (window);
  GstMfxWindowX11Private *const priv2 = GST_MFX_WINDOW_X11_GET_PRIVATE (window);
  Display *const dpy = GST_MFX_DISPLAY_HANDLE (priv2->display);
  const Window xid = GST_MFX_WINDOW_ID (window);
  XWindowAttributes wattr;
  gboolean has_errors;

  if (!priv2->is_mapped)
    return TRUE;

  GST_MFX_DISPLAY_LOCK (priv2->display);
  x11_trap_errors ();
  if (priv->use_foreign_window) {
    XGetWindowAttributes (dpy, xid, &wattr);
    if (!(wattr.your_event_mask & StructureNotifyMask))
      XSelectInput (dpy, xid, StructureNotifyMask);
  }
  XUnmapWindow (dpy, xid);
  has_errors = x11_untrap_errors () != 0;
  GST_MFX_DISPLAY_UNLOCK (priv2->display);

  if (!has_errors) {
    wait_event (window, UnmapNotify);
    if (priv->use_foreign_window &&
        !(wattr.your_event_mask & StructureNotifyMask)) {
      GST_MFX_DISPLAY_LOCK (priv2->display);
      x11_trap_errors ();
      XSelectInput (dpy, xid, wattr.your_event_mask);
      has_errors = x11_untrap_errors () != 0;
      GST_MFX_DISPLAY_UNLOCK (priv2->display);
    }
    priv2->is_mapped = FALSE;
  }
  return !has_errors;
}

static gboolean
gst_mfx_window_x11_create (GstMfxWindow * window, guint * width, guint * height)
{
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE (window);
  GstMfxWindowX11Private *const priv2 = GST_MFX_WINDOW_X11_GET_PRIVATE (window);
  Display *const dpy = GST_MFX_DISPLAY_HANDLE (priv2->display);
  Window xid = GST_MFX_WINDOW_ID (window);
  guint vid = 0;
  Colormap cmap = None;
  XWindowAttributes wattr;
  Atom atoms[2];
  gboolean ok;

  static const char *atom_names[2] = {
    "_NET_WM_STATE",
    "_NET_WM_STATE_FULLSCREEN",
  };

  if (priv->use_foreign_window && xid) {
    GST_MFX_DISPLAY_LOCK (priv2->display);
    XGetWindowAttributes (dpy, xid, &wattr);
    priv2->is_mapped = wattr.map_state == IsViewable;
    ok = x11_get_geometry (dpy, xid, NULL, NULL, width, height, NULL);
    GST_MFX_DISPLAY_UNLOCK (priv2->display);
    return ok;
  }

  GST_MFX_DISPLAY_LOCK (priv2->display);
  XInternAtoms (dpy,
      (char **) atom_names, G_N_ELEMENTS (atom_names), False, atoms);
  priv2->atom_NET_WM_STATE = atoms[0];
  priv2->atom_NET_WM_STATE_FULLSCREEN = atoms[1];

  xid = x11_create_window (dpy, *width, *height, vid, cmap);
  if (xid)
    XRaiseWindow (dpy, xid);
  GST_MFX_DISPLAY_UNLOCK (priv2->display);

  GST_MFX_WINDOW_ID (window) = xid;
  return xid != None;
}

static void
gst_mfx_window_x11_destroy (GstMfxWindow * window)
{
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE (window);
  GstMfxWindowX11Private *const priv2 = GST_MFX_WINDOW_X11_GET_PRIVATE (window);
  Display *const dpy = GST_MFX_DISPLAY_HANDLE (priv2->display);
  const Window xid = GST_MFX_WINDOW_ID (window);

#ifdef HAVE_XRENDER
  if (priv2->picture) {
    GST_MFX_DISPLAY_LOCK (priv2->display);
    XRenderFreePicture (dpy, priv2->picture);
    GST_MFX_DISPLAY_UNLOCK (priv2->display);
    priv2->picture = None;
  }
#endif

  if (xid) {
    if (!priv->use_foreign_window) {
      GST_MFX_DISPLAY_LOCK (priv2->display);
      XDestroyWindow (dpy, xid);
      GST_MFX_DISPLAY_UNLOCK (priv2->display);
    }
    GST_MFX_WINDOW_ID (window) = None;
  }
  gst_mfx_display_replace (&priv2->display, NULL);
  gst_mfx_surface_replace (&priv2->mapped_surface, NULL);
}

static gboolean
gst_mfx_window_x11_get_geometry (GstMfxWindow * window,
    gint * px, gint * py, guint * pwidth, guint * pheight)
{
  GstMfxWindowX11Private *const priv = GST_MFX_WINDOW_X11_GET_PRIVATE (window);
  Display *const dpy = GST_MFX_DISPLAY_HANDLE (priv->display);
  const Window xid = GST_MFX_WINDOW_ID (window);
  gboolean success;

  GST_MFX_DISPLAY_LOCK (priv->display);
  success = x11_get_geometry (dpy, xid, px, py, pwidth, pheight, NULL);
  GST_MFX_DISPLAY_UNLOCK (priv->display);
  return success;
}

static gboolean
gst_mfx_window_x11_set_fullscreen (GstMfxWindow * window, gboolean fullscreen)
{
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE (window);
  GstMfxWindowX11Private *const priv2 = GST_MFX_WINDOW_X11_GET_PRIVATE (window);
  Display *const dpy = GST_MFX_DISPLAY_HANDLE (priv2->display);
  const Window xid = GST_MFX_WINDOW_ID (window);
  XEvent e;
  guint width, height;
  gboolean has_errors;
  GTimeVal now;
  guint64 end_time;

  GST_MFX_DISPLAY_LOCK (priv2->display);
  x11_trap_errors ();
  if (fullscreen) {
    if (!priv2->is_mapped) {
      priv2->fullscreen_on_map = TRUE;

      XChangeProperty (dpy,
          xid,
          priv2->atom_NET_WM_STATE, XA_ATOM, 32,
          PropModeReplace,
          (unsigned char *) &priv2->atom_NET_WM_STATE_FULLSCREEN, 1);
    } else {
      send_wmspec_change_state (window,
          priv2->atom_NET_WM_STATE_FULLSCREEN, TRUE);
    }
  } else {
    if (!priv2->is_mapped) {
      priv2->fullscreen_on_map = FALSE;

      XDeleteProperty (dpy, xid, priv2->atom_NET_WM_STATE);
    } else {
      send_wmspec_change_state (window,
          priv2->atom_NET_WM_STATE_FULLSCREEN, FALSE);
    }
  }
  XSync (dpy, False);
  has_errors = x11_untrap_errors () != 0;
  GST_MFX_DISPLAY_UNLOCK (priv2->display);
  if (has_errors)
    return FALSE;

  /* Try to wait for the completion of the fullscreen mode switch */
  if (!priv->use_foreign_window && priv2->is_mapped) {
    const guint DELAY = 100000; /* 100 ms */
    g_get_current_time (&now);
    end_time = DELAY + ((guint64) now.tv_sec * 1000000 + now.tv_usec);
    while (timed_wait_event (window, ConfigureNotify, end_time, &e)) {
      if (fullscreen) {
        gst_mfx_display_get_size (priv2->display,
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
  GstMfxWindowX11Private *const priv = GST_MFX_WINDOW_X11_GET_PRIVATE (window);
  Display *display = gst_mfx_display_x11_get_display (priv->display);
  gboolean has_errors;

  if (!GST_MFX_WINDOW_ID (window))
    return FALSE;

  GST_MFX_DISPLAY_LOCK (priv->display);
  x11_trap_errors ();
  XResizeWindow (display, GST_MFX_WINDOW_ID (window), width, height);
  has_errors = x11_untrap_errors () != 0;

#ifdef HAVE_XRENDER
  if (priv->picture) {
    XRenderColor color_black = {.red=0, .green=0, .blue=0, .alpha=0xffff};

    XRenderFillRectangle (display, PictOpClear, priv->picture, &color_black,
        0, 0, width, height);
  }
#endif
  GST_MFX_DISPLAY_UNLOCK (priv->display);

  return !has_errors;
}

static gboolean
gst_mfx_window_x11_render (GstMfxWindow * window,
  GstMfxSurface * surface,
  const GstMfxRectangle * src_rect, const GstMfxRectangle * dst_rect)
{
#if defined(USE_DRI3) && defined(HAVE_XCBDRI3) && defined(HAVE_XCBPRESENT) && defined(HAVE_XRENDER)
  GstMfxWindowX11Private *const priv = GST_MFX_WINDOW_X11_GET_PRIVATE (window);
  GstMfxPrimeBufferProxy *buffer_proxy;

  Display *display = gst_mfx_display_x11_get_display (priv->display);
  const Window win = GST_MFX_WINDOW_ID (window);
  Window root;
  int x = 0, y = 0, bpp = 0;
  unsigned int width, height, border, depth, stride, size;
  xcb_pixmap_t pixmap;
  Picture picture;
  XRenderPictFormat *pic_fmt;
  XWindowAttributes wattr;
  int fmt = 0, op = 0;

  if (!priv->xcbconn) {
    GST_MFX_DISPLAY_LOCK (priv->display);
    priv->xcbconn = XGetXCBConnection (display);
    GST_MFX_DISPLAY_UNLOCK (priv->display);
  }

  if (!gst_mfx_surface_has_video_memory (surface)) {
    if (!priv->mapped_surface) {
      GstVideoInfo info;

      gst_video_info_init(&info);
      gst_video_info_set_format(&info, GST_MFX_SURFACE_FORMAT (surface),
        GST_MFX_SURFACE_WIDTH (surface), GST_MFX_SURFACE_HEIGHT (surface));

      priv->mapped_surface = gst_mfx_surface_vaapi_new (
        GST_MFX_WINDOW_GET_PRIVATE (window)->context, &info);
    }

    if (!gst_mfx_surface_map (priv->mapped_surface))
      return FALSE;

    memcpy(gst_mfx_surface_get_plane (priv->mapped_surface, 0),
      gst_mfx_surface_get_plane (surface, 0),
      gst_mfx_surface_get_data_size (surface));

    gst_mfx_surface_unmap (priv->mapped_surface);
  }

  buffer_proxy = gst_mfx_prime_buffer_proxy_new_from_surface (
    priv->mapped_surface ? priv->mapped_surface : surface);
  if (!buffer_proxy)
    return FALSE;

  GST_MFX_DISPLAY_LOCK (priv->display);
  XGetGeometry (display, win, &root, &x, &y,
      &width, &height, &border, &depth);
  GST_MFX_DISPLAY_UNLOCK (priv->display);

  /* Ensure Picture for window created */
  if (!priv->picture) {
    GST_MFX_DISPLAY_LOCK (priv->display);
    XGetWindowAttributes (display, win, &wattr);
    pic_fmt = XRenderFindVisualFormat (display, wattr.visual);
    if (pic_fmt)
      priv->picture = XRenderCreatePicture (display, win, pic_fmt, 0, NULL);
    GST_MFX_DISPLAY_UNLOCK (priv->display);
    if (!priv->picture)
      return FALSE;
  }

  switch (depth) {
    case 8:
      bpp = 8;
      break;
    case 15:
    case 16:
      bpp = 16;
      break;
    case 24:
      fmt = PictStandardRGB24;
      op = PictOpSrc;
      goto get_pic_fmt;
    case 32:
      fmt = PictStandardARGB32;
      op = PictOpOver;
get_pic_fmt:
      bpp = 32;
      GST_MFX_DISPLAY_LOCK (priv->display);
      pic_fmt = XRenderFindStandardFormat (display, fmt);
      GST_MFX_DISPLAY_UNLOCK (priv->display);
      break;
    default:
      break;
  }
  stride = GST_ROUND_UP_16 (src_rect->width) * bpp / 8;
  size = GST_ROUND_UP_N (stride * src_rect->height, 4096);
  if (!pic_fmt) {
    GST_ERROR("Unable to initialize picture format.\n");
    return FALSE;
  }

  GST_MFX_DISPLAY_LOCK (priv->display);
  pixmap = xcb_generate_id (priv->xcbconn);
  xcb_dri3_pixmap_from_buffer (priv->xcbconn, pixmap, win, size,
      src_rect->width, src_rect->height, stride, depth, bpp,
      GST_MFX_PRIME_BUFFER_PROXY_HANDLE (buffer_proxy));
  if (!pixmap) {
    GST_MFX_DISPLAY_UNLOCK (priv->display);
    return FALSE;
  }

  do {
    const double sx = (double) src_rect->width / dst_rect->width;
    const double sy = (double) src_rect->height / dst_rect->height;
    XTransform xform;

    picture = XRenderCreatePicture (display, pixmap, pic_fmt, 0, NULL);
    if (!picture)
      break;

    xform.matrix[0][0] = XDoubleToFixed (sx);
    xform.matrix[0][1] = XDoubleToFixed (0.0);
    xform.matrix[0][2] = XDoubleToFixed (src_rect->x);
    xform.matrix[1][0] = XDoubleToFixed (0.0);
    xform.matrix[1][1] = XDoubleToFixed (sy);
    xform.matrix[1][2] = XDoubleToFixed (src_rect->y);
    xform.matrix[2][0] = XDoubleToFixed (0.0);
    xform.matrix[2][1] = XDoubleToFixed (0.0);
    xform.matrix[2][2] = XDoubleToFixed (1.0);

    XRenderSetPictureTransform (display, picture, &xform);
    XRenderSetPictureFilter (display, picture, FilterBilinear, 0, 0);

    XRenderComposite (display, op, picture, None, priv->picture,
        0, 0, 0, 0, dst_rect->x, dst_rect->y,
        dst_rect->width, dst_rect->height);
  } while (0);

  if (picture)
    XRenderFreePicture (display, picture);
  xcb_free_pixmap (priv->xcbconn, pixmap);
  xcb_flush (priv->xcbconn);

  GST_MFX_DISPLAY_UNLOCK (priv->display);

  gst_mfx_prime_buffer_proxy_unref (buffer_proxy);
  return TRUE;
#else
  GST_ERROR("Unable to render the video.\n");
  return FALSE;
#endif
}

void
gst_mfx_window_x11_class_init (GstMfxWindowX11Class * klass)
{
  GstMfxWindowClass *const window_class = GST_MFX_WINDOW_CLASS (klass);

  window_class->create = gst_mfx_window_x11_create;
  window_class->destroy = gst_mfx_window_x11_destroy;
  window_class->show = gst_mfx_window_x11_show;
  window_class->hide = gst_mfx_window_x11_hide;
  window_class->get_geometry = gst_mfx_window_x11_get_geometry;
  window_class->set_fullscreen = gst_mfx_window_x11_set_fullscreen;
  window_class->resize = gst_mfx_window_x11_resize;
  window_class->render = gst_mfx_window_x11_render;
}

static void
gst_mfx_window_x11_init(GstMfxWindowX11 * window)
{
}

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
gst_mfx_window_x11_new (GstMfxDisplay * display, GstMfxContext * context,
  guint width, guint height)
{
  GstMfxWindowX11 * window;

  g_return_val_if_fail (display != NULL, NULL);
  g_return_val_if_fail (context != NULL, NULL);

  GST_DEBUG ("new window, size %ux%u", width, height);

  window = g_object_new (GST_TYPE_MFX_WINDOW_X11, NULL);
  if (!window)
    return NULL;

  GST_MFX_WINDOW_X11_GET_PRIVATE (window)->display =
      gst_mfx_display_ref (display);

  return
      gst_mfx_window_new_internal (GST_MFX_WINDOW(window),
          context, GST_MFX_ID_INVALID, width, height);
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
  GstMfxWindowX11 * window;

  g_return_val_if_fail (display != NULL, NULL);
  g_return_val_if_fail (xid != None, NULL);

  GST_DEBUG ("new window from xid 0x%08x", (guint) xid);

  window = g_object_new (GST_TYPE_MFX_WINDOW_X11, NULL);
  if (!window)
    return NULL;

  GST_MFX_WINDOW_X11_GET_PRIVATE(window)->display =
      gst_mfx_display_ref (display);

  return gst_mfx_window_new_internal (GST_MFX_WINDOW(window), NULL, xid, 0, 0);
}

void
gst_mfx_window_x11_clear (GstMfxWindow * window)
{
#ifdef HAVE_XRENDER
  GstMfxWindowPrivate *const priv = GST_MFX_WINDOW_GET_PRIVATE (window);
  GstMfxWindowX11Private *const priv2 = GST_MFX_WINDOW_X11_GET_PRIVATE (window);
  Display *display = gst_mfx_display_x11_get_display (priv2->display);

  if (priv2->picture) {
    XRenderColor color_black = {.red=0, .green=0, .blue=0, .alpha=0xffff};

    GST_MFX_DISPLAY_LOCK (priv2->display);
    XRenderFillRectangle (display, PictOpClear, priv2->picture, &color_black,
        0, 0, priv->width, priv->height);
    GST_MFX_DISPLAY_UNLOCK (priv2->display);
  }
#endif
}
