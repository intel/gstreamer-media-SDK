#include "sysdeps.h"
#include "gstmfxobject_priv.h"
#include "gstmfxwindow_drm.h"
#include "gstmfxwindow_priv.h"
#include "gstmfxdisplay_drm_priv.h"

typedef struct _GstMfxWindowDRMClass GstMfxWindowDRMClass;

/**
 * GstMfxWindowDRM:
 *
 * A dummy DRM window abstraction.
 */
struct _GstMfxWindowDRM
{
  /*< private >*/
  GstMfxWindow parent_instance;
};

/**
 * GstMfxWindowDRMClass:
 *
 * A dummy DRM window abstraction class.
 */
struct _GstMfxWindowDRMClass
{
  /*< private >*/
  GstMfxWindowClass parent_instance;
};

static gboolean
gst_mfx_window_drm_show (GstMfxWindow * window)
{
  return TRUE;
}

static gboolean
gst_mfx_window_drm_hide (GstMfxWindow * window)
{
  return TRUE;
}

static gboolean
gst_mfx_window_drm_create (GstMfxWindow * window,
    guint * width, guint * height)
{
  return TRUE;
}

static gboolean
gst_mfx_window_drm_resize (GstMfxWindow * window, guint width, guint height)
{
  return TRUE;
}

static gboolean
gst_mfx_window_drm_render (GstMfxWindow * window,
    GstMfxSurface * surface,
    const GstMfxRectangle * src_rect,
    const GstMfxRectangle * dst_rect, guint flags)
{
  return TRUE;
}

void
gst_mfx_window_drm_class_init (GstMfxWindowDRMClass * klass)
{
  GstMfxWindowClass *const window_class = GST_MFX_WINDOW_CLASS (klass);

  window_class->create = gst_mfx_window_drm_create;
  window_class->show = gst_mfx_window_drm_show;
  window_class->hide = gst_mfx_window_drm_hide;
  window_class->resize = gst_mfx_window_drm_resize;
  window_class->render = gst_mfx_window_drm_render;
}

static void
gst_mfx_window_drm_finalize (GstMfxWindowDRM * window)
{
}

GST_MFX_OBJECT_DEFINE_CLASS_WITH_CODE (GstMfxWindowDRM,
    gst_mfx_window_drm, gst_mfx_window_drm_class_init (&g_class));

/**
 * gst_mfx_window_drm_new:
 * @display: a #GstMfxDisplay
 * @width: the requested window width, in pixels (unused)
 * @height: the requested windo height, in pixels (unused)
 *
 * Creates a dummy window. The window will be attached to the @display.
 * All rendering functions will return success since VA/DRM is a
 * renderless API.
 *
 * Note: this dummy window object is only necessary to fulfill cases
 * where the client application wants to automatically determine the
 * best display to use for the current system. As such, it provides
 * utility functions with the same API (function arguments) to help
 * implement uniform function tables.
 *
 * Return value: the newly allocated #GstMfxWindow object
 */
GstMfxWindow *
gst_mfx_window_drm_new (GstMfxDisplay * display, guint width, guint height)
{
  GST_DEBUG ("new window, size %ux%u", width, height);

  g_return_val_if_fail (GST_MFX_IS_DISPLAY_DRM (display), NULL);

  return
      gst_mfx_window_new_internal (GST_MFX_WINDOW_CLASS
      (gst_mfx_window_drm_class ()), display, width, height);
}
