/*
 *  Copyright (C) 2011-2014 Intel Corporation
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
 *  Copyright (C) 2016 Intel Corporation
 *    Author: Ishmael Visayana Sameen <ishmael.visayana.sameen@intel.com>
 *    Author: Puunithaaraj Gopal <puunithaaraj.gopal@intel.com>
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

#include <gst/gst.h>
#include <gst/video/video.h>

/* Supported interfaces */
#include <gst/video/navigation.h>

#include <gst-libs/mfx/sysdeps.h>
#include "gstmfxsink.h"
#include "gstmfxpluginutil.h"
#include "gstmfxvideometa.h"
#include "gstmfxvideobufferpool.h"
#include "gstmfxvideomemory.h"

#include <gst-libs/mfx/gstmfxsurface.h>
#include <gst-libs/mfx/gstmfxsurfacecomposition.h>

#define GST_PLUGIN_NAME "mfxsink"
#define GST_PLUGIN_DESC "A MFX-based videosink"

GST_DEBUG_CATEGORY_STATIC (gst_debug_mfxsink);
#define GST_CAT_DEFAULT gst_debug_mfxsink

/* Default template */
static const char gst_mfxsink_sink_caps_str[] = GST_MFX_MAKE_SURFACE_CAPS ";";

static GstStaticPadTemplate gst_mfxsink_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (gst_mfxsink_sink_caps_str));

static void
gst_mfxsink_video_overlay_iface_init (GstVideoOverlayInterface * iface);

static void gst_mfxsink_navigation_iface_init (GstNavigationInterface * iface);

G_DEFINE_TYPE_WITH_CODE (GstMfxSink,
    gst_mfxsink,
    GST_TYPE_VIDEO_SINK,
    GST_MFX_PLUGIN_BASE_INIT_INTERFACES
    G_IMPLEMENT_INTERFACE (GST_TYPE_VIDEO_OVERLAY,
        gst_mfxsink_video_overlay_iface_init);
    G_IMPLEMENT_INTERFACE (GST_TYPE_NAVIGATION,
        gst_mfxsink_navigation_iface_init));

enum
{
  HANDOFF_SIGNAL,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_DISPLAY_TYPE,
  PROP_DISPLAY_NAME,
  PROP_FULLSCREEN,
  PROP_FORCE_ASPECT_RATIO,
  PROP_NO_FRAME_DROP,
  PROP_GL_API,
  PROP_FULL_COLOR_RANGE,
  N_PROPERTIES
};

#define DEFAULT_DISPLAY_TYPE            GST_MFX_DISPLAY_TYPE_ANY
#define DEFAULT_GL_API                  GST_MFX_GLAPI_GLES2

static GParamSpec *g_properties[N_PROPERTIES] = { NULL, };

static void gst_mfxsink_video_overlay_expose (GstVideoOverlay * overlay);

static gboolean gst_mfxsink_reconfigure_window (GstMfxSink * sink);

static void
gst_mfxsink_set_event_handling (GstMfxSink * sink, gboolean handle_events);

static GstFlowReturn
gst_mfxsink_show_frame (GstVideoSink * video_sink, GstBuffer * buffer);

static gboolean
gst_mfxsink_ensure_render_rect (GstMfxSink * sink, guint width, guint height);

static inline gboolean
gst_mfxsink_render_surface (GstMfxSink * sink, GstMfxSurface * surface,
    const GstMfxRectangle * surface_rect)
{
  return sink->window && gst_mfx_window_put_surface (sink->window, surface,
      surface_rect, &sink->display_rect);
}

static inline void
gst_mfxsink_lock (GstMfxSink * sink)
{
  if (sink->display_type_req != GST_MFX_DISPLAY_TYPE_EGL)
    GST_MFX_DISPLAY_LOCK (sink->drm_display);
}

static inline void
gst_mfxsink_unlock (GstMfxSink * sink)
{
  if (sink->display_type_req != GST_MFX_DISPLAY_TYPE_EGL)
    GST_MFX_DISPLAY_UNLOCK (sink->drm_display);
}

#ifdef USE_EGL
# include <egl/gstmfxdisplay_egl.h>
# include <egl/gstmfxwindow_egl.h>

#define GST_MFX_TYPE_GL_API \
  gst_mfx_gl_api_get_type ()

static GType
gst_mfx_gl_api_get_type (void)
{
  static GType gl_api_type = 0;

  static const GEnumValue api_types[] = {
    {GST_MFX_GLAPI_OPENGL,
        "Desktop OpenGL", "opengl"},
    {GST_MFX_GLAPI_GLES2,
        "OpenGL ES 2.0", "gles2"},
    {0, NULL, NULL},
  };

  if (!gl_api_type) {
    gl_api_type = g_enum_register_static ("GstMfxGLAPI", api_types);
  }
  return gl_api_type;
}
#endif // USE_EGL

#ifdef WITH_X11
# include <x11/gstmfxdisplay_x11.h>
# include <x11/gstmfxwindow_x11.h>

#ifdef HAVE_XKBLIB
# include <X11/XKBlib.h>
#endif

static inline KeySym
x11_keycode_to_keysym (Display * dpy, unsigned int kc)
{
#ifdef HAVE_XKBLIB
  return XkbKeycodeToKeysym (dpy, kc, 0, 0);
#else
  return XKeycodeToKeysym (dpy, kc, 0);
#endif
}

static gboolean
gst_mfxsink_x11_handle_events (GstMfxSink * sink)
{
  gboolean has_events;
  guint pointer_x = 0, pointer_y = 0;
  gboolean pointer_moved = FALSE;
  XEvent e;

  if (sink->window) {
    GstMfxWindow *window = sink->window;
#ifdef USE_EGL
    if (sink->display_type_req == GST_MFX_DISPLAY_TYPE_EGL)
      window = gst_mfx_window_egl_get_parent_window (sink->window);
#endif
    GstMfxDisplay *const display = GST_MFX_WINDOW_DISPLAY (window);
    Display *const x11_dpy =
        gst_mfx_display_x11_get_display (display);
    Window x11_win = GST_MFX_WINDOW_ID (window);

    /* Track MousePointer interaction */
    for (;;) {
      gst_mfx_display_lock (display);
      has_events = XCheckWindowEvent (x11_dpy, x11_win, PointerMotionMask, &e);
      gst_mfx_display_unlock (display);
      if (!has_events)
        break;
      switch (e.type) {
        case MotionNotify:
          pointer_x = e.xmotion.x;
          pointer_y = e.xmotion.y;
          pointer_moved = TRUE;
          break;
        default:
          break;
      }
    }
    if (pointer_moved) {
      gst_mfx_display_lock (display);
      gst_navigation_send_mouse_event (GST_NAVIGATION (sink),
          "mouse-move", 0, pointer_x, pointer_y);
      gst_mfx_display_unlock (display);
    }
    /* Track KeyPress, KeyRelease, ButtonPress, ButtonRelease */
    for (;;) {
      KeySym keysym;
      const char *key_str = NULL;
      gst_mfx_display_lock (display);
      has_events = XCheckWindowEvent (x11_dpy, x11_win,
          KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask,
          &e);
      gst_mfx_display_unlock (display);
      if (!has_events)
        break;
      switch (e.type) {
        case ButtonPress:
          gst_navigation_send_mouse_event (GST_NAVIGATION (sink),
              "mouse-button-press", e.xbutton.button, e.xbutton.x, e.xbutton.y);
          break;
        case ButtonRelease:
          gst_navigation_send_mouse_event (GST_NAVIGATION (sink),
              "mouse-button-release", e.xbutton.button, e.xbutton.x,
              e.xbutton.y);
          break;
        case KeyPress:
        case KeyRelease:
          gst_mfx_display_lock (display);
          keysym = x11_keycode_to_keysym (x11_dpy, e.xkey.keycode);
          if (keysym != NoSymbol) {
            key_str = XKeysymToString (keysym);
          } else {
            key_str = "unknown";
          }
          gst_mfx_display_unlock (display);
          gst_navigation_send_key_event (GST_NAVIGATION (sink),
              e.type == KeyPress ? "key-press" : "key-release", key_str);
          break;
        default:
          break;
      }
    }
    /* Handle Expose + ConfigureNotify */
    /* Need to lock whole loop or we corrupt the XEvent queue: */
    for (;;) {
      gst_mfx_display_lock (display);
      has_events = XCheckWindowEvent (x11_dpy, x11_win,
          StructureNotifyMask | ExposureMask, &e);
      gst_mfx_display_unlock (display);
      if (!has_events)
        break;
      switch (e.type) {
        case Expose:
        case ConfigureNotify:
          gst_mfxsink_reconfigure_window (sink);
          break;
        default:
          break;
      }
    }
  }
  return TRUE;
}

static gboolean
gst_mfxsink_x11_pre_start_event_thread (GstMfxSink * sink)
{
  static const int x11_event_mask = (KeyPressMask | KeyReleaseMask |
      ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
      ExposureMask | StructureNotifyMask);

  if (sink->window) {
    GstMfxWindow *window = sink->window;
#ifdef USE_EGL
    if (sink->display_type_req == GST_MFX_DISPLAY_TYPE_EGL)
      window = gst_mfx_window_egl_get_parent_window (sink->window);
#endif
    GstMfxDisplay *const display = GST_MFX_WINDOW_DISPLAY (window);

    gst_mfx_display_lock (GST_MFX_DISPLAY (display));
    XSelectInput (gst_mfx_display_x11_get_display (display),
        GST_MFX_WINDOW_ID (window), x11_event_mask);
    gst_mfx_display_unlock (GST_MFX_DISPLAY (display));
  }
  return TRUE;
}

static gboolean
gst_mfxsink_x11_pre_stop_event_thread (GstMfxSink * sink)
{
  if (sink->window) {
    GstMfxWindow *window = sink->window;
#ifdef USE_EGL
    if (sink->display_type_req == GST_MFX_DISPLAY_TYPE_EGL)
      window = gst_mfx_window_egl_get_parent_window (sink->window);
#endif
    GstMfxDisplay *const display = GST_MFX_WINDOW_DISPLAY (window);

    gst_mfx_display_lock (GST_MFX_DISPLAY (display));
    XSelectInput (gst_mfx_display_x11_get_display (display),
        GST_MFX_WINDOW_ID (window), 0);
    gst_mfx_display_unlock (GST_MFX_DISPLAY (display));
  }
  return TRUE;
}

/* Checks whether a ConfigureNotify event is in the queue */
typedef struct _ConfigureNotifyEventPendingArgs ConfigureNotifyEventPendingArgs;
struct _ConfigureNotifyEventPendingArgs
{
  Window window;
  guint width;
  guint height;
  gboolean match;
};

static Bool
configure_notify_event_pending_cb (Display * dpy, XEvent * xev, XPointer arg)
{
  ConfigureNotifyEventPendingArgs *const args =
      (ConfigureNotifyEventPendingArgs *) arg;

  if (xev->type == ConfigureNotify &&
      xev->xconfigure.window == args->window &&
      xev->xconfigure.width == args->width &&
      xev->xconfigure.height == args->height)
    args->match = TRUE;

  /* XXX: this is a hack to traverse the whole queue because we
     can't use XPeekIfEvent() since it could block */
  return False;
}

static gboolean
configure_notify_event_pending (GstMfxSink * sink, Window window,
    guint width, guint height)
{
  GstMfxDisplay *const x11_display = GST_MFX_DISPLAY (sink->display);
  ConfigureNotifyEventPendingArgs args;
  XEvent xev;

  args.window = window;
  args.width = width;
  args.height = height;
  args.match = FALSE;

  /* XXX: don't use XPeekIfEvent() because it might block */
  XCheckIfEvent (gst_mfx_display_x11_get_display (x11_display),
      &xev, configure_notify_event_pending_cb, (XPointer) & args);
  return args.match;
}

/* ------------------------------------------------------------------------ */
/* --- X11 Backend                                                      --- */
/* -------------------------------------------------------------------------*/

static gboolean
gst_mfxsink_x11_create_window (GstMfxSink * sink, guint width, guint height)
{
  XID xid = 0;

  if (sink->foreign_window == TRUE && sink->app_window_handle != 0) {
    xid = sink->app_window_handle;
    sink->window = gst_mfx_window_x11_new_with_xid (sink->display, xid);
  } else {
    sink->window = gst_mfx_window_x11_new (sink->display, width, height);
  }

  if (!sink->window)
    return FALSE;

  return TRUE;
}

static gboolean
gst_mfxsink_x11_create_window_from_handle (GstMfxSink * sink,
    guintptr window)
{
  Window rootwin;
  unsigned int width, height, border_width, depth;
  int x, y;
  XID xid = window;

  gst_mfx_display_lock (sink->display);
  XGetGeometry (gst_mfx_display_x11_get_display (GST_MFX_DISPLAY
          (sink->display)), xid, &rootwin, &x, &y, &width, &height, &border_width,
      &depth);
  gst_mfx_display_unlock (sink->display);

  if ((width != sink->window_width || height != sink->window_height) &&
      !configure_notify_event_pending (sink, xid, width, height)) {
    if (!gst_mfxsink_ensure_render_rect (sink, width, height))
      return FALSE;
    sink->window_width = width;
    sink->window_height = height;
  }

  if (!sink->window || (Window)(GST_MFX_WINDOW_ID (sink->window)) != xid) {
    gst_mfx_window_replace (&sink->window, NULL);
    sink->window = gst_mfx_window_x11_new_with_xid (sink->display, xid);
    if (!sink->window)
      return FALSE;
  }
  gst_mfx_window_x11_clear (sink->window);
  gst_mfxsink_set_event_handling (sink, sink->handle_events);

  return TRUE;
}

static const inline GstMfxSinkBackend *
gst_mfxsink_backend_x11 (void)
{
  static const GstMfxSinkBackend GstMfxSinkBackendX11 = {
    .create_window = gst_mfxsink_x11_create_window,
    .create_window_from_handle = gst_mfxsink_x11_create_window_from_handle,
    .handle_events = gst_mfxsink_x11_handle_events,
    .pre_start_event_thread = gst_mfxsink_x11_pre_start_event_thread,
    .pre_stop_event_thread = gst_mfxsink_x11_pre_stop_event_thread,
  };
  return &GstMfxSinkBackendX11;
}
#endif // WITH_X11


/* ------------------------------------------------------------------------ */
/* --- EGL Backend                                                      --- */
/* ------------------------------------------------------------------------ */

#ifdef USE_EGL
static gboolean
gst_mfxsink_egl_create_window (GstMfxSink * sink, guint width, guint height)
{
  g_return_val_if_fail (sink->window == NULL, FALSE);
  sink->window = gst_mfx_window_egl_new (sink->display, width, height);
  if (!sink->window)
    return FALSE;
  return TRUE;
}

static const inline GstMfxSinkBackend *
gst_mfxsink_backend_egl (void)
{
  static const GstMfxSinkBackend GstMfxSinkBackendEGL = {
    .create_window = gst_mfxsink_egl_create_window,
#ifdef WITH_X11
    .handle_events = gst_mfxsink_x11_handle_events,
    .pre_start_event_thread = gst_mfxsink_x11_pre_start_event_thread,
    .pre_stop_event_thread = gst_mfxsink_x11_pre_stop_event_thread,
#endif
  };
  return &GstMfxSinkBackendEGL;
}
#endif // USE_EGL

/* ------------------------------------------------------------------------ */
/* --- Wayland Backend                                                  --- */
/* -------------------------------------------------------------------------*/
#ifdef WITH_WAYLAND
# include <wayland/gstmfxdisplay_wayland.h>
# include <wayland/gstmfxwindow_wayland.h>

static gboolean
gst_mfxsink_wayland_create_window (GstMfxSink * sink, guint width, guint height)
{
  g_return_val_if_fail (sink->window == NULL, FALSE);
  sink->window = gst_mfx_window_wayland_new (sink->display, width, height);
  if (!sink->window)
    return FALSE;
  return TRUE;
}

static const inline GstMfxSinkBackend *
gst_mfxsink_backend_wayland (void)
{
  static const GstMfxSinkBackend GstMfxSinkBackendWayland = {
    .create_window = gst_mfxsink_wayland_create_window,
  };
  return &GstMfxSinkBackendWayland;
}
#endif


/* ------------------------------------------------------------------------ */
/* --- GstVideoOverlay interface                                        --- */
/* ------------------------------------------------------------------------ */

static void
gst_mfxsink_video_overlay_set_window_handle (GstVideoOverlay * overlay,
    guintptr window)
{
  GstMfxSink *const sink = GST_MFXSINK (overlay);

  sink->foreign_window = TRUE;
  sink->app_window_handle = window;
  if (sink->backend && sink->backend->create_window_from_handle)
    sink->backend->create_window_from_handle (sink, window);
}

static void
gst_mfxsink_video_overlay_set_render_rectangle (GstVideoOverlay * overlay,
    gint x, gint y, gint width, gint height)
{
  GstMfxSink *const sink = GST_MFXSINK (overlay);
  GstMfxRectangle *const display_rect = &sink->display_rect;

  display_rect->x = x;
  display_rect->y = y;
  display_rect->width = width;
  display_rect->height = height;

  GST_DEBUG ("render rect (%d,%d):%ux%u",
      display_rect->x, display_rect->y,
      display_rect->width, display_rect->height);
}

static void
gst_mfxsink_video_overlay_expose (GstVideoOverlay * overlay)
{
  GstMfxSink *const sink = GST_MFXSINK (overlay);

  gst_mfxsink_reconfigure_window (sink);
}

static void
gst_mfxsink_video_overlay_set_event_handling (GstVideoOverlay * overlay,
    gboolean handle_events)
{
  GstMfxSink *const sink = GST_MFXSINK (overlay);

  gst_mfxsink_set_event_handling (sink, handle_events);
}

static void
gst_mfxsink_video_overlay_iface_init (GstVideoOverlayInterface * iface)
{
  iface->set_window_handle = gst_mfxsink_video_overlay_set_window_handle;
  iface->set_render_rectangle =
      gst_mfxsink_video_overlay_set_render_rectangle;
  iface->expose = gst_mfxsink_video_overlay_expose;
  iface->handle_events = gst_mfxsink_video_overlay_set_event_handling;
}

/* ------------------------------------------------------------------------ */
/* --- GstNavigation interface                                          --- */
/* ------------------------------------------------------------------------ */

static void
gst_mfxsink_navigation_send_event (GstNavigation * navigation,
    GstStructure * structure)
{
  GstMfxSink *const sink = GST_MFXSINK (navigation);
  GstPad *peer;

  if ((peer = gst_pad_get_peer (GST_MFX_PLUGIN_BASE_SINK_PAD (sink)))) {
    GstEvent *event;
    GstMfxRectangle *disp_rect = &sink->display_rect;
    gdouble x, y, xscale = 1.0, yscale = 1.0;

    event = gst_event_new_navigation (structure);

    if (!sink->window)
      return;

    /* We calculate scaling using the original video frames geometry to include
       pixel aspect ratio scaling. */
    xscale = (gdouble) sink->video_width / disp_rect->width;
    yscale = (gdouble) sink->video_height / disp_rect->height;

    /* Converting pointer coordinates to the non scaled geometry */
    if (gst_structure_get_double (structure, "pointer_x", &x)) {
      x = MIN (x, disp_rect->x + disp_rect->width);
      x = MAX (x - disp_rect->x, 0);
      gst_structure_set (structure, "pointer_x", G_TYPE_DOUBLE,
          (gdouble) x * xscale, NULL);
    }
    if (gst_structure_get_double (structure, "pointer_y", &y)) {
      y = MIN (y, disp_rect->y + disp_rect->height);
      y = MAX (y - disp_rect->y, 0);
      gst_structure_set (structure, "pointer_y", G_TYPE_DOUBLE,
          (gdouble) y * yscale, NULL);
    }

    gst_pad_send_event (peer, event);
    gst_object_unref (peer);
  }
}

static void
gst_mfxsink_navigation_iface_init (GstNavigationInterface * iface)
{
  iface->send_event = gst_mfxsink_navigation_send_event;
}

/* ------------------------------------------------------------------------ */
/* --- Common implementation                                            --- */
/* ------------------------------------------------------------------------ */

static gboolean
gst_mfxsink_reconfigure_window (GstMfxSink * sink)
{
  guint win_width, win_height;

  if (sink->window) {
    gst_mfx_window_reconfigure (sink->window);
    gst_mfx_window_get_size (sink->window, &win_width, &win_height);
    if (win_width != sink->window_width || win_height != sink->window_height) {
      if (!gst_mfxsink_ensure_render_rect (sink, win_width, win_height))
        return FALSE;
      GST_INFO ("window was resized from %ux%u to %ux%u",
          sink->window_width, sink->window_height, win_width, win_height);
      sink->window_width = win_width;
      sink->window_height = win_height;
      return TRUE;
    }
  }
  return FALSE;
}

static gpointer
gst_mfxsink_event_thread (GstMfxSink * sink)
{
  GST_OBJECT_LOCK (sink);
  while (!sink->event_thread_cancel) {
    GST_OBJECT_UNLOCK (sink);
    sink->backend->handle_events (sink);
    g_usleep (G_USEC_PER_SEC / 20);
    GST_OBJECT_LOCK (sink);
  }
  GST_OBJECT_UNLOCK (sink);
  return NULL;
}

static void
gst_mfxsink_set_event_handling (GstMfxSink * sink, gboolean handle_events)
{
  GThread *thread = NULL;

  if ((sink->display_type != GST_MFX_DISPLAY_TYPE_X11) || !sink->backend)
    return;

  GST_OBJECT_LOCK (sink);
  sink->handle_events = handle_events;
  if (handle_events && !sink->event_thread) {
    /* Setup our event listening thread */
    GST_DEBUG ("starting xevent thread");
    if (sink->backend->pre_start_event_thread)
      sink->backend->pre_start_event_thread (sink);

    sink->event_thread_cancel = FALSE;
    sink->event_thread = g_thread_try_new ("mfxsink-events",
        (GThreadFunc) gst_mfxsink_event_thread, sink, NULL);
  } else if (!handle_events && sink->event_thread) {
    GST_DEBUG ("stopping xevent thread");
    if (sink->backend->pre_stop_event_thread)
      sink->backend->pre_stop_event_thread (sink);

    /* Grab thread and mark it as NULL */
    thread = sink->event_thread;
    sink->event_thread = NULL;
    sink->event_thread_cancel = TRUE;
  }
  GST_OBJECT_UNLOCK (sink);

  /* Wait for our event thread to finish */
  if (thread) {
    g_thread_join (thread);
    GST_DEBUG ("xevent thread stopped");
  }
}

static const gchar *
get_display_type_name (GstMfxDisplayType display_type)
{
  gpointer const klass = g_type_class_peek (GST_MFX_TYPE_DISPLAY_TYPE);
  GEnumValue *const e = g_enum_get_value (klass, display_type);

  if (e)
    return e->value_name;
  return "<unknown-type>";
}

static void
gst_mfxsink_set_display_name (GstMfxSink * sink, const gchar * display_name)
{
  g_free (sink->display_name);
  sink->display_name = g_strdup (display_name);
}

static void
gst_mfxsink_set_render_backend (GstMfxSink * sink)
{
  GstMfxDisplay *display = NULL;

  switch (sink->display_type_req) {
#ifdef USE_DRI3
    case GST_MFX_DISPLAY_TYPE_X11:
      display = gst_mfx_display_x11_new (sink->display_name);
      if (!display)
        goto display_unsupported;
      sink->backend = gst_mfxsink_backend_x11 ();
      sink->display_type = GST_MFX_DISPLAY_TYPE_X11;
      break;
#endif
#ifdef USE_WAYLAND
    case GST_MFX_DISPLAY_TYPE_WAYLAND:
      if (!sink->display) {
        display = gst_mfx_display_wayland_new (sink->display_name);
        if (!display)
          goto display_unsupported;
      }
      sink->backend = gst_mfxsink_backend_wayland ();
      sink->display_type = GST_MFX_DISPLAY_TYPE_WAYLAND;
      break;
#endif
#ifdef USE_EGL
    case GST_MFX_DISPLAY_TYPE_EGL:
      display = gst_mfx_display_egl_new (sink->display_name, sink->gl_api);
      if (!display)
        goto display_unsupported;
      sink->backend = gst_mfxsink_backend_egl ();
      sink->display_type =
          GST_MFX_DISPLAY_TYPE (gst_mfx_display_egl_get_parent_display (display));
      break;
#endif
display_unsupported:
    default:
      GST_ERROR ("display type %s not supported",
          get_display_type_name (sink->display_type_req));
      g_assert_not_reached ();
      break;
  }

  if (display) {
    gst_mfx_display_replace (&sink->display, display);
    gst_mfx_display_unref (display);
  }
}

static gboolean
gst_mfxsink_ensure_render_rect (GstMfxSink * sink, guint width, guint height)
{
  GstMfxRectangle *const display_rect = &sink->display_rect;
  guint num, den, display_par_n, display_par_d;
  gboolean success;

  /* Return success if caps are not set yet */
  if (!sink->caps)
    return TRUE;

  if (!sink->keep_aspect) {
    display_rect->width = width;
    display_rect->height = height;
    display_rect->x = 0;
    display_rect->y = 0;

    GST_DEBUG ("force-aspect-ratio is false; distorting while scaling video");
    GST_DEBUG ("render rect (%d,%d):%ux%u",
        display_rect->x, display_rect->y,
        display_rect->width, display_rect->height);
    return TRUE;
  }

  GST_DEBUG ("ensure render rect within %ux%u bounds", width, height);

  gst_mfx_display_get_pixel_aspect_ratio (sink->display,
      &display_par_n, &display_par_d);
  GST_DEBUG ("display pixel-aspect-ratio %d/%d", display_par_n, display_par_d);

  success = gst_video_calculate_display_ratio (&num, &den,
      sink->video_width, sink->video_height,
      sink->video_par_n, sink->video_par_d, display_par_n, display_par_d);
  if (!success)
    return FALSE;
  GST_DEBUG ("video size %dx%d, calculated ratio %d/%d",
      sink->video_width, sink->video_height, num, den);

  if (sink->fullscreen) {
    display_rect->width = width;
    display_rect->height= height;
  } else {
    display_rect->width = gst_util_uint64_scale_int (height, num, den);
    if (display_rect->width <= width) {
      GST_DEBUG ("keeping window height");
      display_rect->height = height;
    } else {
      GST_DEBUG ("keeping window width");
      display_rect->width = width;
      display_rect->height = gst_util_uint64_scale_int (width, den, num);
    }
  }

  GST_DEBUG ("scaling video to %ux%u", display_rect->width,
      display_rect->height);

  g_assert (display_rect->width <= width);
  g_assert (display_rect->height <= height);

  display_rect->x = (width - display_rect->width) / 2;
  display_rect->y = (height - display_rect->height) / 2;

  GST_DEBUG ("render rect (%d,%d):%ux%u",
      display_rect->x, display_rect->y,
      display_rect->width, display_rect->height);

  return TRUE;
}

static inline gboolean
gst_mfxsink_ensure_window (GstMfxSink * sink, guint width, guint height)
{
  return sink->window || sink->backend->create_window (sink, width, height);
}

static void
gst_mfxsink_ensure_window_size (GstMfxSink * sink, guint * width_ptr,
    guint * height_ptr)
{
  GstVideoRectangle src_rect, dst_rect, out_rect;
  guint num, den, display_width, display_height, display_par_n, display_par_d;
  gboolean success, scale;

  if (sink->foreign_window) {
    *width_ptr = sink->window_width;
    *height_ptr = sink->window_height;
    return;
  }

  gst_mfx_display_get_size (sink->display, &display_width, &display_height);
  if (sink->fullscreen) {
    *width_ptr = display_width;
    *height_ptr = display_height;
    return;
  }

  gst_mfx_display_get_pixel_aspect_ratio (sink->display,
      &display_par_n, &display_par_d);

  success = gst_video_calculate_display_ratio (&num, &den,
      sink->video_width, sink->video_height,
      sink->video_par_n, sink->video_par_d, display_par_n, display_par_d);
  if (!success) {
    num = sink->video_par_n;
    den = sink->video_par_d;
  }

  src_rect.x = 0;
  src_rect.y = 0;
  src_rect.w = gst_util_uint64_scale_int (sink->video_height, num, den);
  src_rect.h = sink->video_height;
  dst_rect.x = 0;
  dst_rect.y = 0;
  dst_rect.w = display_width;
  dst_rect.h = display_height;
  scale = (src_rect.w > dst_rect.w || src_rect.h > dst_rect.h);
  gst_video_sink_center_rect (src_rect, dst_rect, &out_rect, scale);
  *width_ptr = out_rect.w;
  *height_ptr = out_rect.h;
}

static gboolean
gst_mfxsink_start (GstBaseSink * base_sink)
{
  GstMfxSink *const sink = GST_MFXSINK_CAST (base_sink);
  GstMfxPluginBase *plugin = GST_MFX_PLUGIN_BASE (sink);

  if (!gst_mfx_plugin_base_ensure_aggregator (plugin))
    return FALSE;

  sink->drm_display =
      gst_mfx_task_aggregator_get_display (plugin->aggregator);

  return TRUE;
}

static gboolean
gst_mfxsink_stop (GstBaseSink * base_sink)
{
  GstMfxSink *const sink = GST_MFXSINK_CAST (base_sink);

  if (!sink->foreign_window) {
    gst_mfx_window_replace (&sink->window, NULL);
    gst_mfx_display_replace (&sink->display, NULL);
  }

  gst_mfx_composite_filter_replace (&sink->composite_filter, NULL);
  gst_mfx_display_replace (&sink->drm_display, NULL);

  gst_mfx_plugin_base_close (GST_MFX_PLUGIN_BASE (sink));
  return TRUE;
}

static GstCaps *
gst_mfxsink_get_caps_impl (GstBaseSink * base_sink)
{
  GstMfxSink *const sink = GST_MFXSINK_CAST (base_sink);
  GstCaps *out_caps;

  if (sink->display_type_req == GST_MFX_DISPLAY_TYPE_ANY) {
#ifdef WITH_WAYLAND
    GstMfxDisplay *display = gst_mfx_display_wayland_new (sink->display_name);
    if (display) {
# ifdef USE_WAYLAND
      sink->display_type_req = GST_MFX_DISPLAY_TYPE_WAYLAND;
# else
      sink->display_type_req = GST_MFX_DISPLAY_TYPE_EGL;
# endif
      gst_mfx_display_replace (&sink->display, display);
      gst_mfx_display_unref (display);
    }
    else
#endif  // WITH_WAYLAND
#ifdef USE_DRI3
      sink->display_type_req = GST_MFX_DISPLAY_TYPE_X11;
#else
      sink->display_type_req = GST_MFX_DISPLAY_TYPE_EGL;
#endif // USE_DRI3
  }

  if (sink->display_type_req == GST_MFX_DISPLAY_TYPE_X11 ||
      sink->display_type_req == GST_MFX_DISPLAY_TYPE_EGL ||
      sink->full_color_range)
    out_caps =
        gst_mfx_video_format_new_template_caps_with_features
        (GST_VIDEO_FORMAT_BGRA, GST_CAPS_FEATURE_MEMORY_MFX_SURFACE);
  else
    out_caps =
        gst_static_pad_template_get_caps (&gst_mfxsink_sink_factory);

  return out_caps;
}

static inline GstCaps *
gst_mfxsink_get_caps (GstBaseSink * base_sink, GstCaps * filter)
{
  GstCaps *caps, *out_caps;

  caps = gst_mfxsink_get_caps_impl (base_sink);
  if (caps && filter) {
    out_caps = gst_caps_intersect_full (caps, filter, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
  } else
    out_caps = caps;

  return out_caps;
}

static gboolean
gst_mfxsink_set_caps (GstBaseSink * base_sink, GstCaps * caps)
{
  GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE (base_sink);
  GstMfxSink *const sink = GST_MFXSINK_CAST (base_sink);
  GstVideoInfo *const vip = GST_MFX_PLUGIN_BASE_SINK_PAD_INFO (sink);
  guint win_width, win_height;

  gst_mfxsink_set_render_backend (sink);

  if (!gst_mfx_plugin_base_set_caps (plugin, caps, NULL))
    return FALSE;

  sink->video_width = GST_VIDEO_INFO_WIDTH (vip);
  sink->video_height = GST_VIDEO_INFO_HEIGHT (vip);
  sink->video_par_n = GST_VIDEO_INFO_PAR_N (vip);
  sink->video_par_d = GST_VIDEO_INFO_PAR_D (vip);
  GST_DEBUG ("video pixel-aspect-ratio %d/%d",
      sink->video_par_n, sink->video_par_d);

  gst_caps_replace (&sink->caps, caps);

  gst_mfxsink_ensure_window_size (sink, &win_width, &win_height);
  if (sink->window) {
    if (!sink->foreign_window || sink->fullscreen)
      gst_mfx_window_set_size (sink->window, win_width, win_height);
  } else {
    gst_mfx_display_lock (sink->display);
    gst_video_overlay_prepare_window_handle (GST_VIDEO_OVERLAY (sink));
    gst_mfx_display_unlock (sink->display);
    if (sink->window)
      return TRUE;
    if (!gst_mfxsink_ensure_window (sink, win_width, win_height))
      return FALSE;
    gst_mfx_window_set_fullscreen (sink->window, sink->fullscreen);
    gst_mfx_window_show (sink->window);
    gst_mfx_window_get_size (sink->window, &win_width, &win_height);
    gst_mfxsink_set_event_handling (sink, sink->handle_events);
  }
  sink->window_width = win_width;
  sink->window_height = win_height;
  GST_DEBUG ("window size %ux%u", win_width, win_height);

  if (sink->no_frame_drop)
    gst_base_sink_set_max_lateness (base_sink, -1);

  return gst_mfxsink_ensure_render_rect (sink, win_width, win_height);
}

static GstFlowReturn
gst_mfxsink_show_frame (GstVideoSink * video_sink, GstBuffer * src_buffer)
{
  GstMfxSink *const sink = GST_MFXSINK_CAST (video_sink);
  GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE (sink);
  GstMfxVideoMeta *meta;
  GstMfxSurface *surface, *composite_surface = NULL;
  GstMfxRectangle *surface_rect = NULL;
  GstFlowReturn ret;

  GstVideoOverlayCompositionMeta *const cmeta =
      gst_buffer_get_video_overlay_composition_meta (src_buffer);
  GstVideoOverlayComposition *overlay = NULL;
  GstMfxSurfaceComposition *composition = NULL;

  meta = gst_buffer_get_mfx_video_meta (src_buffer);

  surface = gst_mfx_video_meta_get_surface (meta);
  if (!surface)
    goto no_surface;

  GST_DEBUG ("render surface %" GST_MFX_ID_FORMAT,
      GST_MFX_ID_ARGS(GST_MFX_SURFACE_ID (surface)));

  surface_rect = (GstMfxRectangle *)
      gst_mfx_surface_get_crop_rect (surface);

  GST_DEBUG ("render rect (%d,%d), size %ux%u",
      surface_rect->x, surface_rect->y,
      surface_rect->width, surface_rect->height);

  if (cmeta) {
    overlay = cmeta->overlay;

    if (!sink->composite_filter)
      sink->composite_filter =
        gst_mfx_composite_filter_new (plugin->aggregator,
            !gst_mfx_surface_has_video_memory (surface));

    composition = gst_mfx_surface_composition_new (surface, overlay);
    if (!composition) {
      GST_ERROR("Failed to create new surface composition");
      goto error;
    }

    gst_mfx_composite_filter_apply_composition (sink->composite_filter,
        composition, &composite_surface);
  }

  gst_mfxsink_lock (sink);
  if (!gst_mfxsink_render_surface (sink,
        composite_surface ? composite_surface : surface, surface_rect))
    goto error;

  gst_mfx_surface_dequeue(surface);
  ret = GST_FLOW_OK;
done:
  gst_mfxsink_unlock (sink);
  gst_mfx_surface_composition_replace (&composition, NULL);
  return ret;

error:
  GST_ELEMENT_ERROR (sink, RESOURCE, WRITE,
      ("Internal error: could not render surface"), (NULL));
  ret = GST_FLOW_ERROR;
  goto done;

no_surface:
  /* No surface or surface surface. That's very bad! */
  GST_WARNING_OBJECT (sink, "could not get surface");
  ret = GST_FLOW_ERROR;
  goto done;
}

static gboolean
gst_mfxsink_query (GstBaseSink * base_sink, GstQuery * query)
{
  GstMfxSink *const sink = GST_MFXSINK_CAST (base_sink);
  GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE (sink);
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      ret = gst_mfx_handle_context_query (query, plugin->aggregator);
      break;
    default:
      ret = GST_BASE_SINK_CLASS (gst_mfxsink_parent_class)->query (base_sink,
          query);
      break;
  }

  return ret;
}

static void
gst_mfxsink_destroy (GstMfxSink * sink)
{
  gst_mfxsink_set_event_handling (sink, FALSE);

  gst_mfx_window_replace (&sink->window, NULL);
  gst_mfx_display_replace (&sink->display, NULL);

  gst_caps_replace (&sink->caps, NULL);

  sink->app_window_handle = 0;

  g_free (sink->display_name);
}

static void
gst_mfxsink_finalize (GObject * object)
{
  gst_mfxsink_destroy (GST_MFXSINK_CAST (object));

  gst_mfx_plugin_base_finalize (GST_MFX_PLUGIN_BASE (object));
  G_OBJECT_CLASS (gst_mfxsink_parent_class)->finalize (object);
}

static void
gst_mfxsink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstMfxSink *const sink = GST_MFXSINK_CAST (object);

  switch (prop_id) {
    case PROP_DISPLAY_TYPE:
      sink->display_type_req = g_value_get_enum (value);
      break;
    case PROP_DISPLAY_NAME:
      gst_mfxsink_set_display_name (sink, g_value_get_string (value));
      break;
    case PROP_FULLSCREEN:
      sink->fullscreen = g_value_get_boolean (value);
      break;
    case PROP_FORCE_ASPECT_RATIO:
      sink->keep_aspect = g_value_get_boolean (value);
      break;
    case PROP_NO_FRAME_DROP:
      sink->no_frame_drop = g_value_get_boolean (value);
      break;
    case PROP_FULL_COLOR_RANGE:
      sink->full_color_range = g_value_get_boolean (value);
      break;
    case PROP_GL_API:
      sink->gl_api = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mfxsink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstMfxSink *const sink = GST_MFXSINK_CAST (object);

  switch (prop_id) {
    case PROP_DISPLAY_TYPE:
      g_value_set_enum (value, sink->display_type);
      break;
    case PROP_DISPLAY_NAME:
      g_value_set_string (value, sink->display_name);
      break;
    case PROP_FULLSCREEN:
      g_value_set_boolean (value, sink->fullscreen);
      break;
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, sink->keep_aspect);
      break;
    case PROP_NO_FRAME_DROP:
      g_value_set_boolean (value, sink->no_frame_drop);
      break;
    case PROP_FULL_COLOR_RANGE:
      g_value_set_boolean (value, sink->full_color_range);
      break;
    case PROP_GL_API:
      g_value_set_enum (value, sink->gl_api);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mfxsink_class_init (GstMfxSinkClass * klass)
{
  GObjectClass *const object_class = G_OBJECT_CLASS (klass);
  GstElementClass *const element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *const basesink_class = GST_BASE_SINK_CLASS (klass);
  GstVideoSinkClass *const videosink_class = GST_VIDEO_SINK_CLASS (klass);
  GstMfxPluginBaseClass *const base_plugin_class =
      GST_MFX_PLUGIN_BASE_CLASS (klass);
  GstPadTemplate *pad_template;

  GST_DEBUG_CATEGORY_INIT (gst_debug_mfxsink,
      GST_PLUGIN_NAME, 0, GST_PLUGIN_DESC);

  gst_mfx_plugin_base_class_init (base_plugin_class);

  object_class->finalize = gst_mfxsink_finalize;
  object_class->set_property = gst_mfxsink_set_property;
  object_class->get_property = gst_mfxsink_get_property;

  basesink_class->start = gst_mfxsink_start;
  basesink_class->stop = gst_mfxsink_stop;
  basesink_class->get_caps = gst_mfxsink_get_caps;
  basesink_class->set_caps = gst_mfxsink_set_caps;
  basesink_class->query = GST_DEBUG_FUNCPTR (gst_mfxsink_query);

  videosink_class->show_frame = GST_DEBUG_FUNCPTR (gst_mfxsink_show_frame);

  gst_element_class_set_static_metadata (element_class,
      "MFX sink", "Sink/Video", GST_PLUGIN_DESC,
      "Ishmael Sameen <ishmael.visayana.sameen@intel.com>");

  pad_template = gst_static_pad_template_get (&gst_mfxsink_sink_factory);
  gst_element_class_add_pad_template (element_class, pad_template);

  /**
   * GstMfxSink:display:
   *
   * The type of display to use.
   */
  g_properties[PROP_DISPLAY_TYPE] =
      g_param_spec_enum ("display",
      "display type",
      "display type to use",
      GST_MFX_TYPE_DISPLAY_TYPE,
      GST_MFX_DISPLAY_TYPE_ANY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMfxSink:display-name:
   *
   * The native display name.
   */
  g_properties[PROP_DISPLAY_NAME] =
      g_param_spec_string ("display-name",
      "display name",
      "display name to use", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMfxSink:fullscreen:
   *
   * Selects whether fullscreen mode is enabled or not.
   */
  g_properties[PROP_FULLSCREEN] =
      g_param_spec_boolean ("fullscreen",
      "Fullscreen",
      "Requests window in fullscreen state",
      FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMfxSink:force-aspect-ratio:
   *
   * When enabled, scaling respects video aspect ratio; when disabled,
   * the video is distorted to fit the window.
   */
  g_properties[PROP_FORCE_ASPECT_RATIO] =
      g_param_spec_boolean ("force-aspect-ratio",
      "Force aspect ratio",
      "When enabled, scaling will respect original aspect ratio",
      TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMfxSink:no-frame-drop:
   *
   * When enabled, all decoded frames arriving at the sink will be rendered
   * regardless of its lateness. This option helps to deal with slow initial
   * render times and possible frame drops when rendering the first few frames.
   */
  g_properties[PROP_NO_FRAME_DROP] =
      g_param_spec_boolean ("no-frame-drop",
      "No frame drop",
      "Render all decoded frames when enabled, ignoring timestamp lateness",
      FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMfxSink:full-color-range:
   *
   * When enabled, all decoded frames will be in RGB 0-255.
   */
  g_properties[PROP_FULL_COLOR_RANGE] =
      g_param_spec_boolean ("full-color-range",
      "Full color range",
      "Decoded frames will be in RGB 0-255",
      FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
#ifdef USE_EGL
  /**
   * GstMfxSink:gl-api:
   *
   * Select the OpenGL API for rendering
   */
  g_properties[PROP_GL_API] =
      g_param_spec_enum ("gl-api",
      "OpenGL API",
      "OpenGL API to use",
      GST_MFX_TYPE_GL_API,
      DEFAULT_GL_API, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
#endif
  g_object_class_install_properties (object_class, N_PROPERTIES, g_properties);
}

static void
gst_mfxsink_init (GstMfxSink * sink)
{
  GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE (sink);

  gst_mfx_plugin_base_init (plugin, GST_CAT_DEFAULT);
  sink->display_type_req = DEFAULT_DISPLAY_TYPE;
  sink->gl_api = DEFAULT_GL_API;
  sink->display_name = NULL;

  sink->video_par_n = 1;
  sink->video_par_d = 1;
  sink->handle_events = TRUE;
  sink->keep_aspect = TRUE;
  sink->no_frame_drop = FALSE;
  sink->full_color_range = FALSE;
  sink->app_window_handle = 0;
  gst_video_info_init (&sink->video_info);
}
