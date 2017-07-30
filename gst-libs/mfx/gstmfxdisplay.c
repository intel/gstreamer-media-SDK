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

#include <fcntl.h>
#include <libudev.h>
#include <xf86drm.h>
#include <va/va_drm.h>
#include "gstmfxdisplay.h"
#include "gstmfxdisplay_priv.h"
#include "gstmfxutils_vaapi.h"

#define DEBUG 1
#include "gstmfxdebug.h"

G_DEFINE_TYPE_WITH_CODE(GstMfxDisplay,
  gst_mfx_display,
  GST_TYPE_OBJECT,
  G_ADD_PRIVATE(GstMfxDisplay));

static int
get_display_fd (GstMfxDisplay * display)
{
  GstMfxDisplayPrivate *const priv = GST_MFX_DISPLAY_GET_PRIVATE (display);
  const gchar *sysnames[] = { "renderD[0-9]*", "card[0-9]*", NULL };
  const gchar *syspath, *devpath;
  struct udev *udev = NULL;
  struct udev_device *device, *parent;
  struct udev_enumerate *e = NULL;
  struct udev_list_entry *l;
  guint i;

  if (!priv->display_fd) {
    udev = udev_new ();
    if (!udev)
      goto end;

    e = udev_enumerate_new (udev);
    if (!e)
      goto end;

    udev_enumerate_add_match_subsystem (e, "drm");

    for (i = 0; sysnames[i]; i++) {
      udev_enumerate_add_match_sysname (e, sysnames[i]);
      udev_enumerate_scan_devices (e);
      udev_list_entry_foreach (l, udev_enumerate_get_list_entry (e)) {
        syspath = udev_list_entry_get_name (l);
        device = udev_device_new_from_syspath (udev, syspath);
        parent = udev_device_get_parent (device);

        if (strcmp (udev_device_get_subsystem (parent), "pci") != 0) {
          udev_device_unref (device);
          continue;
        }

        devpath = udev_device_get_devnode (device);
        priv->display_fd = open (devpath, O_RDWR | O_CLOEXEC);
        if (priv->display_fd < 0) {
          udev_device_unref (device);
          continue;
        }

        udev_device_unref (device);
        break;
      }
      if (priv->display_fd)
        break;
    }
  }

end:
  if (e)
    udev_enumerate_unref (e);
  if (udev)
    udev_unref (udev);
  return priv->display_fd;
}

/* GstMfxDisplayType enumerations */
GType
gst_mfx_display_type_get_type (void)
{
  static GType g_type = 0;

  static const GEnumValue display_types[] = {
    {GST_MFX_DISPLAY_TYPE_ANY,
        "Auto detection", "any"},
#ifdef USE_DRI3
    {GST_MFX_DISPLAY_TYPE_X11,
        "X11 display", "x11"},
#endif
#ifdef USE_WAYLAND
    {GST_MFX_DISPLAY_TYPE_WAYLAND,
        "Wayland display", "wayland"},
#endif
    {0, NULL, NULL},
  };

  if (!g_type)
    g_type = g_enum_register_static ("GstMfxDisplayType", display_types);
  return g_type;
}

static void
gst_mfx_display_calculate_pixel_aspect_ratio (GstMfxDisplay * display)
{
  GstMfxDisplayPrivate *const priv = GST_MFX_DISPLAY_GET_PRIVATE (display);
  gdouble ratio, delta;
  gint i, j, index, windex;

  static const gint par[][2] = {
    {1, 1},                     /* regular screen            */
    {16, 15},                   /* PAL TV                    */
    {11, 10},                   /* 525 line Rec.601 video    */
    {54, 59},                   /* 625 line Rec.601 video    */
    {64, 45},                   /* 1280x1024 on 16:9 display */
    {5, 3},                     /* 1280x1024 on  4:3 display */
    {4, 3}                      /*  800x600  on 16:9 display */
  };

  /* First, calculate the "real" ratio based on the X values;
   * which is the "physical" w/h divided by the w/h in pixels of the
   * display */
  if (!priv->width || !priv->height || !priv->width_mm || !priv->height_mm)
    ratio = 1.0;
  else
    ratio = (gdouble) (priv->width_mm * priv->height) /
        (priv->height_mm * priv->width);
  GST_DEBUG ("calculated pixel aspect ratio: %f", ratio);

  /* Now, find the one from par[][2] with the lowest delta to the real one */
#define DELTA(idx, w) (ABS(ratio - ((gdouble)par[idx][w] / par[idx][!(w)])))
  delta = DELTA (0, 0);
  index = 0;
  windex = 0;

  for (i = 1; i < G_N_ELEMENTS (par); i++) {
    for (j = 0; j < 2; j++) {
      const gdouble this_delta = DELTA (i, j);
      if (this_delta < delta) {
        index = i;
        windex = j;
        delta = this_delta;
      }
    }
  }
#undef DELTA

  priv->par_n = par[index][windex];
  priv->par_d = par[index][windex ^ 1];
}

static void
gst_mfx_display_destroy (GstMfxDisplay * display)
{
  GstMfxDisplayPrivate *const priv = GST_MFX_DISPLAY_GET_PRIVATE (display);
  GstMfxDisplayClass *klass = GST_MFX_DISPLAY_GET_CLASS (display);

  if (priv->va_display) {
    vaTerminate (priv->va_display);
    priv->va_display = NULL;
  }
  if (priv->display_fd) {
    close (priv->display_fd);
    priv->display_fd = 0;
  }
  if (klass->close_display)
    klass->close_display (display);
}

static gboolean
gst_mfx_display_create (GstMfxDisplay * display, gpointer init_value)
{
  GstMfxDisplayPrivate *const priv = GST_MFX_DISPLAY_GET_PRIVATE (display);
  const GstMfxDisplayClass *const klass = GST_MFX_DISPLAY_GET_CLASS (display);

  if (klass->open_display && !klass->open_display(display, init_value))
    return FALSE;
  if (klass->get_size)
    klass->get_size (display, &priv->width, &priv->height);
  if (klass->get_size_mm)
    klass->get_size_mm (display, &priv->width_mm, &priv->height_mm);
  gst_mfx_display_calculate_pixel_aspect_ratio (display);

  return TRUE;
}

static gboolean
gst_mfx_display_init_vaapi (GstMfxDisplay * display)
{
  GstMfxDisplayPrivate *const priv = GST_MFX_DISPLAY_GET_PRIVATE (display);
  gint major_version, minor_version;
  VAStatus status;

  priv->va_display = vaGetDisplayDRM (get_display_fd (display));
  if (!priv->va_display)
    return FALSE;

  status = vaInitialize (priv->va_display, &major_version, &minor_version);
  if (!vaapi_check_status (status, "vaInitialize()"))
    return FALSE;

  GST_DEBUG ("VA-API version %d.%d", major_version, minor_version);

  return TRUE;
}

/**
 * gst_mfx_display_lock:
 * @display: a #GstMfxDisplay
 *
 * Locks @display. If @display is already locked by another thread,
 * the current thread will block until @display is unlocked by the
 * other thread.
 */
void
gst_mfx_display_lock (GstMfxDisplay * display)
{
  GstMfxDisplayPrivate *priv = GST_MFX_DISPLAY_GET_PRIVATE (display);

  g_rec_mutex_lock (&priv->mutex);
}

/**
 * gst_mfx_display_unlock:
 * @display: a #GstMfxDisplay
 *
 * Unlocks @display. If another thread is blocked in a
 * gst_mfx_display_lock() call for @display, it will be woken and
 * can lock @display itself.
 */
void
gst_mfx_display_unlock (GstMfxDisplay * display)
{
  GstMfxDisplayPrivate *priv = GST_MFX_DISPLAY_GET_PRIVATE (display);

  g_rec_mutex_unlock (&priv->mutex);
}

static void
gst_mfx_display_init (GstMfxDisplay * display)
{
  GstMfxDisplayPrivate *const priv =
    gst_mfx_display_get_instance_private(display);
  const GstMfxDisplayClass *const dpy_class =
      GST_MFX_DISPLAY_GET_CLASS (display);

  priv->display_type = GST_MFX_DISPLAY_TYPE_ANY;
  priv->par_n = 1;
  priv->par_d = 1;

  g_rec_mutex_init (&priv->mutex);

  display->priv = priv;

  if (dpy_class->init)
    dpy_class->init (display);
}

static void
gst_mfx_display_finalize (GObject * object)
{
  GstMfxDisplay* display = GST_MFX_DISPLAY(object);
  GstMfxDisplayPrivate *const priv = GST_MFX_DISPLAY_GET_PRIVATE (display);

  gst_mfx_display_destroy (display);
  g_rec_mutex_clear (&priv->mutex);
}

void
gst_mfx_display_class_init (GstMfxDisplayClass * klass)
{
  GObjectClass *const object_class = G_OBJECT_CLASS(klass);

  GST_DEBUG_CATEGORY_INIT (gst_debug_mfx, "mfx", 0, "MFX helper");

  object_class->finalize = gst_mfx_display_finalize;
}

GstMfxDisplay *
gst_mfx_display_new_internal (GstMfxDisplay *display, gpointer init_value)
{
  if (!gst_mfx_display_create (display, init_value))
    goto error;
  return display;

error:
  gst_mfx_display_unref (display);
  return NULL;
}

GstMfxDisplay *
gst_mfx_display_new (void)
{
  GstMfxDisplay *display;

  GST_DEBUG ("creating VAAPI display");

  display = g_object_new(GST_TYPE_MFX_DISPLAY, NULL);
  if (!display)
    return NULL;

  if (!gst_mfx_display_init_vaapi(display))
    goto error;
  return display;

error:
  gst_mfx_display_unref (display);
  return NULL;
}

GstMfxDisplay *
gst_mfx_display_ref (GstMfxDisplay * display)
{
  g_return_val_if_fail(display != NULL, NULL);

  return gst_object_ref (GST_OBJECT(display));
}

void
gst_mfx_display_unref (GstMfxDisplay * display)
{
  gst_object_unref (GST_OBJECT(display));
}

void
gst_mfx_display_replace (GstMfxDisplay ** old_display_ptr,
    GstMfxDisplay * new_display)
{
  g_return_if_fail(old_display_ptr != NULL);

  gst_object_replace ((GstObject **)old_display_ptr,
    GST_OBJECT(new_display));
}

/**
 * gst_mfx_display_get_display_type:
 * @display: a #GstMfxDisplay
 *
 * Returns the #GstMfxDisplayType of the VA display bound to
 * @display. This is not the type of the @display object.
 *
 * Return value: the #GstMfxDisplayType
 */
GstMfxDisplayType
gst_mfx_display_get_display_type (GstMfxDisplay * display)
{
  g_return_val_if_fail (display != NULL, GST_MFX_DISPLAY_TYPE_ANY);

  return GST_MFX_DISPLAY_GET_CLASS_TYPE (display);
}

/**
 * gst_mfx_display_get_display:
 * @display: a #GstMfxDisplay
 *
 * Returns the #VADisplay bound to @display.
 *
 * Return value: the #VADisplay
 */
VADisplay
gst_mfx_display_get_vadisplay (GstMfxDisplay * display)
{
  g_return_val_if_fail (display != NULL, 0);

  return GST_MFX_DISPLAY_GET_PRIVATE (display)->va_display;
}

/**
 * gst_mfx_display_get_size:
 * @display: a #GstMfxDisplay
 * @pwidth: return location for the width, or %NULL
 * @pheight: return location for the height, or %NULL
 *
 * Retrieves the dimensions of a #GstMfxDisplay.
 */
void
gst_mfx_display_get_size (GstMfxDisplay * display, guint * pwidth,
    guint * pheight)
{
  g_return_if_fail (GST_MFX_DISPLAY (display));

  if (pwidth)
    *pwidth = GST_MFX_DISPLAY_GET_PRIVATE (display)->width;

  if (pheight)
    *pheight = GST_MFX_DISPLAY_GET_PRIVATE (display)->height;
}

/**
 * gst_mfx_display_get_pixel_aspect_ratio:
 * @display: a #GstMfxDisplay
 * @par_n: return location for the numerator of pixel aspect ratio, or %NULL
 * @par_d: return location for the denominator of pixel aspect ratio, or %NULL
 *
 * Retrieves the pixel aspect ratio of a #GstMfxDisplay.
 */
void
gst_mfx_display_get_pixel_aspect_ratio (GstMfxDisplay * display,
    guint * par_n, guint * par_d)
{
  g_return_if_fail (display != NULL);

  if (par_n)
    *par_n = GST_MFX_DISPLAY_GET_PRIVATE (display)->par_n;

  if (par_d)
    *par_d = GST_MFX_DISPLAY_GET_PRIVATE (display)->par_d;
}
