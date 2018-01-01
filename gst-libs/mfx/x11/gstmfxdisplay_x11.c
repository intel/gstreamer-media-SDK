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

#include "gstmfxdisplay_x11.h"
#include "gstmfxdisplay_x11_priv.h"
#include "gstmfxwindow_x11.h"

#ifdef HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif

#define DEBUG 1
#include "gstmfxdebug.h"

G_DEFINE_TYPE (GstMfxDisplayX11, gst_mfx_display_x11, GST_TYPE_MFX_DISPLAY);

static inline const gchar *
get_default_display_name (void)
{
  static const gchar *g_display_name;

  if (!g_display_name)
    g_display_name = getenv ("DISPLAY");
  return g_display_name;
}

static void
set_display_name (GstMfxDisplayX11 * display, const gchar * display_name)
{
  GstMfxDisplayX11Private *const priv = &display->priv;

  g_free (priv->display_name);

  if (!display_name) {
    display_name = get_default_display_name ();
    if (!display_name)
      return;
  }
  priv->display_name = g_strdup (display_name);
}

/* Check for display server extensions */
static void
check_extensions (GstMfxDisplay * display)
{
  GstMfxDisplayX11Private *const priv = GST_MFX_DISPLAY_X11_PRIVATE (display);
  Display *const dpy = GST_MFX_DISPLAY_HANDLE (display);
  int evt_base, err_base;

#ifdef HAVE_XRANDR
  priv->use_xrandr = XRRQueryExtension (dpy, &evt_base, &err_base);
#endif
}

static gboolean
gst_mfx_display_x11_open_display (GstMfxDisplay * base_display,
    const gchar * name)
{
  GstMfxDisplayX11 *const display = GST_MFX_DISPLAY_X11_CAST (base_display);
  GstMfxDisplayX11Private *const priv = &display->priv;

  set_display_name (display, name);

  GST_MFX_DISPLAY_HANDLE (base_display) = XOpenDisplay (priv->display_name);
  if (!GST_MFX_DISPLAY_HANDLE (base_display))
    return FALSE;

  priv->x11_screen = DefaultScreen (GST_MFX_DISPLAY_HANDLE (base_display));

  check_extensions (base_display);
  return TRUE;
}

static void
gst_mfx_display_x11_close_display (GstMfxDisplay * display)
{
  GstMfxDisplayX11Private *const priv = GST_MFX_DISPLAY_X11_PRIVATE (display);

  if (GST_MFX_DISPLAY_HANDLE (display)) {
    XCloseDisplay (GST_MFX_DISPLAY_HANDLE (display));
    GST_MFX_DISPLAY_HANDLE (display) = NULL;
  }

  if (priv->display_name) {
    g_free (priv->display_name);
    priv->display_name = NULL;
  }
}

static void
gst_mfx_display_x11_get_size (GstMfxDisplay * display,
    guint * pwidth, guint * pheight)
{
  GstMfxDisplayX11Private *const priv = GST_MFX_DISPLAY_X11_PRIVATE (display);
  Display *const dpy = GST_MFX_DISPLAY_HANDLE (display);

  if (!dpy)
    return;

  if (pwidth)
    *pwidth = DisplayWidth (dpy, priv->x11_screen);

  if (pheight)
    *pheight = DisplayHeight (dpy, priv->x11_screen);
}

static void
gst_mfx_display_x11_get_size_mm (GstMfxDisplay * display,
    guint * pwidth, guint * pheight)
{
  GstMfxDisplayX11Private *const priv = GST_MFX_DISPLAY_X11_PRIVATE (display);
  Display *const dpy = GST_MFX_DISPLAY_HANDLE (display);
  guint width_mm, height_mm;

  if (!dpy)
    return;

  width_mm = DisplayWidthMM (dpy, priv->x11_screen);
  height_mm = DisplayHeightMM (dpy, priv->x11_screen);

#ifdef HAVE_XRANDR
  /* XXX: fix up physical size if the display is rotated */
  if (priv->use_xrandr) {
    XRRScreenConfiguration *xrr_config = NULL;
    XRRScreenSize *xrr_sizes;
    Window win;
    int num_xrr_sizes, size_id, screen;
    Rotation rotation;

    do {
      win = DefaultRootWindow (dpy);
      screen = XRRRootToScreen (dpy, win);

      xrr_config = XRRGetScreenInfo (dpy, win);
      if (!xrr_config)
        break;

      size_id = XRRConfigCurrentConfiguration (xrr_config, &rotation);
      if (rotation == RR_Rotate_0 || rotation == RR_Rotate_180)
        break;

      xrr_sizes = XRRSizes (dpy, screen, &num_xrr_sizes);
      if (!xrr_sizes || size_id >= num_xrr_sizes)
        break;

      width_mm = xrr_sizes[size_id].mheight;
      height_mm = xrr_sizes[size_id].mwidth;
    } while (0);
    if (xrr_config)
      XRRFreeScreenConfigInfo (xrr_config);
  }
#endif

  if (pwidth)
    *pwidth = width_mm;

  if (pheight)
    *pheight = height_mm;
}

static void
gst_mfx_display_x11_finalize (GObject * object)
{
  G_OBJECT_CLASS (gst_mfx_display_x11_parent_class)->finalize (object);
}

static void
gst_mfx_display_x11_class_init (GstMfxDisplayX11Class * klass)
{
  GstMfxDisplayClass *const dpy_class = GST_MFX_DISPLAY_CLASS (klass);
  GObjectClass *const object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gst_mfx_display_x11_finalize;

  dpy_class->display_type = GST_MFX_DISPLAY_TYPE_X11;
  dpy_class->open_display = gst_mfx_display_x11_open_display;
  dpy_class->close_display = gst_mfx_display_x11_close_display;
  dpy_class->get_size = gst_mfx_display_x11_get_size;
  dpy_class->get_size_mm = gst_mfx_display_x11_get_size_mm;
}

static void
gst_mfx_display_x11_init (GstMfxDisplayX11 * display)
{
}

/**
 * gst_mfx_display_x11_new:
 * @display_name: the X11 display name
 *
 * Opens an X11 #Display using @display_name and returns a newly
 * allocated #GstMfxDisplay object. The X11 display will be cloed
 * when the reference count of the object reaches zero.
 *
 * Return value: a newly allocated #GstMfxDisplay object
 */
GstMfxDisplay *
gst_mfx_display_x11_new (const gchar * display_name)
{
  GstMfxDisplayX11 *display;

  display = g_object_new (GST_TYPE_MFX_DISPLAY_X11, NULL);
  if (!display)
    return NULL;

  return gst_mfx_display_new_internal (GST_MFX_DISPLAY (display),
      (gpointer) display_name);
}

/**
 * gst_mfx_display_x11_get_display:
 * @display: a #GstMfxDisplayX11
 *
 * Returns the underlying X11 #Display that was created by
 * gst_mfx_display_x11_new() or that was bound from
 * gst_mfx_display_x11_new_with_display().
 *
 * Return value: the X11 #Display attached to @display
 */
Display *
gst_mfx_display_x11_get_display (GstMfxDisplay * display)
{
  g_return_val_if_fail (GST_MFX_IS_DISPLAY_X11 (display), NULL);

  return GST_MFX_DISPLAY_HANDLE (display);
}
