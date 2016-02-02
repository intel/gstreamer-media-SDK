#include <gst/gst.h>
#include <gst/video/video.h>

/* Supported interfaces */
# include <gst/video/navigation.h>

#include "sysdeps.h"
#include "gstmfxsink.h"
#include "gstmfxpluginutil.h"
#include "gstmfxvideometa.h"
#include "gstmfxvideobufferpool.h"
#include "gstmfxvideomemory.h"

#define GST_PLUGIN_NAME "mfxsink"
#define GST_PLUGIN_DESC "A MFX-based videosink"

GST_DEBUG_CATEGORY_STATIC(gst_debug_mfxsink);
#define GST_CAT_DEFAULT gst_debug_mfxsink

/* Default template */
/* *INDENT-OFF* */
static const char gst_mfxsink_sink_caps_str[] =
	GST_MFX_MAKE_ENC_SURFACE_CAPS ";"
	GST_VIDEO_CAPS_MAKE(GST_VIDEO_FORMATS_ALL);
/* *INDENT-ON* */

static GstStaticPadTemplate gst_mfxsink_sink_factory =
GST_STATIC_PAD_TEMPLATE("sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(gst_mfxsink_sink_caps_str));

static void
gst_mfxsink_navigation_iface_init(GstNavigationInterface * iface);

G_DEFINE_TYPE_WITH_CODE(GstMfxSink,
	gst_mfxsink,
	GST_TYPE_VIDEO_SINK,
	GST_MFX_PLUGIN_BASE_INIT_INTERFACES
	G_IMPLEMENT_INTERFACE(GST_TYPE_NAVIGATION,
		gst_mfxsink_navigation_iface_init));

enum
{
	HANDOFF_SIGNAL,
	LAST_SIGNAL
};

static guint gst_mfxsink_signals[LAST_SIGNAL] = { 0 };

enum
{
	PROP_0,
	PROP_DISPLAY_TYPE,
	PROP_DISPLAY_NAME,
	PROP_FULLSCREEN,
	PROP_FORCE_ASPECT_RATIO,
	PROP_SIGNAL_HANDOFFS,
	N_PROPERTIES
};

#define DEFAULT_DISPLAY_TYPE            GST_MFX_DISPLAY_TYPE_EGL
#define DEFAULT_SIGNAL_HANDOFFS         FALSE

static GParamSpec *g_properties[N_PROPERTIES] = { NULL, };

//static void gst_mfxsink_video_overlay_expose(GstVideoOverlay * overlay);

static gboolean gst_mfxsink_reconfigure_window(GstMfxSink * sink);

static void
gst_mfxsink_set_event_handling(GstMfxSink * sink, gboolean handle_events);

static GstFlowReturn
gst_mfxsink_show_frame(GstVideoSink * video_sink, GstBuffer * buffer);

static gboolean
gst_mfxsink_ensure_render_rect(GstMfxSink * sink, guint width,
	guint height);

static inline gboolean
gst_mfxsink_ensure_display(GstMfxSink * sink)
{
	return gst_mfx_plugin_base_ensure_display(GST_MFX_PLUGIN_BASE(sink));
}

static inline gboolean
gst_mfxsink_render_surface(GstMfxSink * sink, GstMfxSurface * surface,
	const GstMfxRectangle * surface_rect)
{
	return sink->window && gst_mfx_window_put_surface(sink->window, surface,
		surface_rect, &sink->display_rect);
}

/* ------------------------------------------------------------------------ */
/* --- X11 Backend                                                      --- */
/* ------------------------------------------------------------------------ */

#if USE_X11
#include "gstmfxdisplay_x11.h"
#include "gstmfxwindow_x11.h"

#if HAVE_XKBLIB
# include <X11/XKBlib.h>
#endif

static inline KeySym
x11_keycode_to_keysym(Display * dpy, unsigned int kc)
{
#if HAVE_XKBLIB
	return XkbKeycodeToKeysym(dpy, kc, 0, 0);
#else
	return XKeycodeToKeysym(dpy, kc, 0);
#endif
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
configure_notify_event_pending_cb(Display * dpy, XEvent * xev, XPointer arg)
{
	ConfigureNotifyEventPendingArgs *const args =
		(ConfigureNotifyEventPendingArgs *)arg;
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
configure_notify_event_pending(GstMfxSink * sink, Window window,
guint width, guint height)
{
	GstMfxDisplayX11 *const display =
		GST_MFX_DISPLAY_X11(GST_MFX_PLUGIN_BASE_DISPLAY(sink));
	ConfigureNotifyEventPendingArgs args;
	XEvent xev;
	args.window = window;
	args.width = width;
	args.height = height;
	args.match = FALSE;
	/* XXX: don't use XPeekIfEvent() because it might block */
	XCheckIfEvent(gst_mfx_display_x11_get_display(display),
		&xev, configure_notify_event_pending_cb, (XPointer)& args);
	return args.match;
}

static gboolean
gst_mfxsink_x11_create_window(GstMfxSink * sink, guint width, guint height)
{
	GstMfxDisplay *const display = GST_MFX_PLUGIN_BASE_DISPLAY(sink);
	g_return_val_if_fail(sink->window == NULL, FALSE);
	sink->window = gst_mfx_window_x11_new(display, width, height);
	if (!sink->window)
		return FALSE;
	return TRUE;
}

static gboolean
gst_mfxsink_x11_create_window_from_handle(GstMfxSink * sink,
	guintptr window)
{
	GstMfxDisplay *display;
	Window rootwin;
	unsigned int width, height, border_width, depth;
	int x, y;
	XID xid = window;
	if (!gst_mfxsink_ensure_display(sink))
		return FALSE;
	display = GST_MFX_PLUGIN_BASE_DISPLAY(sink);
	gst_mfx_display_lock(display);
	XGetGeometry(gst_mfx_display_x11_get_display(GST_MFX_DISPLAY_X11
		(display)), xid, &rootwin, &x, &y, &width, &height, &border_width,
		&depth);
	gst_mfx_display_unlock(display);
	if ((width != sink->window_width || height != sink->window_height) &&
		!configure_notify_event_pending(sink, xid, width, height)) {
		if (!gst_mfxsink_ensure_render_rect(sink, width, height))
			return FALSE;
		sink->window_width = width;
		sink->window_height = height;
	}
	/*if (!sink->window
		|| gst_mfx_window_x11_get_xid(GST_MFX_WINDOW_X11(sink->window)) !=
		xid) {
		gst_mfx_window_replace(&sink->window, NULL);
		sink->window = gst_mfx_window_x11_new_with_xid(display, xid);
		if (!sink->window)
			return FALSE;
	}*/
	gst_mfxsink_set_event_handling(sink, sink->handle_events);
	return TRUE;
}

static gboolean
gst_mfxsink_x11_handle_events(GstMfxSink * sink)
{
	GstMfxDisplay *const display = GST_MFX_PLUGIN_BASE_DISPLAY(sink);
	gboolean has_events, do_expose = FALSE;
	guint pointer_x = 0, pointer_y = 0;
	gboolean pointer_moved = FALSE;
	XEvent e;
	if (sink->window) {
		Display *const x11_dpy =
			gst_mfx_display_x11_get_display(GST_MFX_DISPLAY_X11(display));
		Window x11_win =
			gst_mfx_window_x11_get_xid(GST_MFX_WINDOW_X11(sink->window));
		/* Track MousePointer interaction */
		for (;;) {
			gst_mfx_display_lock(display);
			has_events = XCheckWindowEvent(x11_dpy, x11_win, PointerMotionMask, &e);
			gst_mfx_display_unlock(display);
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
			gst_mfx_display_lock(display);
			gst_navigation_send_mouse_event(GST_NAVIGATION(sink),
				"mouse-move", 0, pointer_x, pointer_y);
			gst_mfx_display_unlock(display);
		}
		/* Track KeyPress, KeyRelease, ButtonPress, ButtonRelease */
		for (;;) {
			KeySym keysym;
			const char *key_str = NULL;
			gst_mfx_display_lock(display);
			has_events = XCheckWindowEvent(x11_dpy, x11_win,
				KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask,
				&e);
			gst_mfx_display_unlock(display);
			if (!has_events)
				break;
			switch (e.type) {
			case ButtonPress:
				gst_navigation_send_mouse_event(GST_NAVIGATION(sink),
					"mouse-button-press", e.xbutton.button, e.xbutton.x, e.xbutton.y);
				break;
			case ButtonRelease:
				gst_navigation_send_mouse_event(GST_NAVIGATION(sink),
					"mouse-button-release", e.xbutton.button, e.xbutton.x,
					e.xbutton.y);
				break;
			case KeyPress:
			case KeyRelease:
				gst_mfx_display_lock(display);
				keysym = x11_keycode_to_keysym(x11_dpy, e.xkey.keycode);
				if (keysym != NoSymbol) {
					key_str = XKeysymToString(keysym);
				}
				else {
					key_str = "unknown";
				}
				gst_mfx_display_unlock(display);
				gst_navigation_send_key_event(GST_NAVIGATION(sink),
					e.type == KeyPress ? "key-press" : "key-release", key_str);
				break;
			default:
				break;
			}
		}
		/* Handle Expose + ConfigureNotify */
		/* Need to lock whole loop or we corrupt the XEvent queue: */
		for (;;) {
			gst_mfx_display_lock(display);
			has_events = XCheckWindowEvent(x11_dpy, x11_win,
				StructureNotifyMask | ExposureMask, &e);
			gst_mfx_display_unlock(display);
			if (!has_events)
				break;
			switch (e.type) {
			case Expose:
				do_expose = TRUE;
				break;
			case ConfigureNotify:
				if (gst_mfxsink_reconfigure_window(sink))
					do_expose = TRUE;
				break;
			default:
				break;
			}
		}
		//if (do_expose)
			//gst_mfxsink_video_overlay_expose(GST_VIDEO_OVERLAY(sink));
	}
	return TRUE;
}

static gboolean
gst_mfxsink_x11_pre_start_event_thread(GstMfxSink * sink)
{
	GstMfxDisplayX11 *const display =
		GST_MFX_DISPLAY_X11(GST_MFX_PLUGIN_BASE_DISPLAY(sink));
	static const int x11_event_mask = (KeyPressMask | KeyReleaseMask |
		ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
		ExposureMask | StructureNotifyMask);
	if (sink->window) {
		gst_mfx_display_lock(GST_MFX_DISPLAY(display));
		XSelectInput(gst_mfx_display_x11_get_display(display),
			gst_mfx_window_x11_get_xid(GST_MFX_WINDOW_X11(sink->window)),
			x11_event_mask);
		gst_mfx_display_unlock(GST_MFX_DISPLAY(display));
	}
	return TRUE;
}

static gboolean
gst_mfxsink_x11_pre_stop_event_thread(GstMfxSink * sink)
{
	GstMfxDisplayX11 *const display =
		GST_MFX_DISPLAY_X11(GST_MFX_PLUGIN_BASE_DISPLAY(sink));
	if (sink->window) {
		gst_mfx_display_lock(GST_MFX_DISPLAY(display));
		XSelectInput(gst_mfx_display_x11_get_display(display),
			gst_mfx_window_x11_get_xid(GST_MFX_WINDOW_X11(sink->window)), 0);
		gst_mfx_display_unlock(GST_MFX_DISPLAY(display));
	}
	return TRUE;
}

/*static const inline GstMfxSinkBackend *
gst_mfxsink_backend_x11(void)
{
	static const GstMfxSinkBackend GstMfxSinkBackendX11 = {
		.create_window = gst_mfxsink_x11_create_window,
		.create_window_from_handle = gst_mfxsink_x11_create_window_from_handle,
		.render_surface = gst_mfxsink_render_surface,
		.event_thread_needed = TRUE,
		.handle_events = gst_mfxsink_x11_handle_events,
		.pre_start_event_thread = gst_mfxsink_x11_pre_start_event_thread,
		.pre_stop_event_thread = gst_mfxsink_x11_pre_stop_event_thread,
	};
	return &GstMfxSinkBackendX11;
}*/
#endif

/* ------------------------------------------------------------------------ */
/* --- EGL Backend                                                  --- */
/* ------------------------------------------------------------------------ */

#if USE_EGL
#include "gstmfxdisplay_egl.h"
#include "gstmfxwindow_egl.h"

static gboolean
gst_mfxsink_egl_create_window(GstMfxSink * sink, guint width,
	guint height)
{
	GstMfxDisplay *display = GST_MFX_PLUGIN_BASE_DISPLAY(sink);

	g_return_val_if_fail(sink->window == NULL, FALSE);
	sink->window = gst_mfx_window_egl_new(display, width, height);
	if (!sink->window)
		return FALSE;
	return TRUE;
}

static const inline GstMfxSinkBackend *
gst_mfxsink_backend_egl(void)
{
	static const GstMfxSinkBackend GstMfxSinkBackendEGL = {
		.create_window = gst_mfxsink_egl_create_window,
		.render_surface = gst_mfxsink_render_surface,
	};
	return &GstMfxSinkBackendEGL;
}
#endif

/* ------------------------------------------------------------------------ */
/* --- GstNavigation interface                                          --- */
/* ------------------------------------------------------------------------ */

static void
gst_mfxsink_navigation_send_event(GstNavigation * navigation,
    GstStructure * structure)
{
	GstMfxSink *const sink = GST_MFXSINK(navigation);
	GstPad *peer;

	if ((peer = gst_pad_get_peer(GST_MFX_PLUGIN_BASE_SINK_PAD(sink)))) {
		GstEvent *event;
		GstMfxRectangle *disp_rect = &sink->display_rect;
		gdouble x, y, xscale = 1.0, yscale = 1.0;

		event = gst_event_new_navigation(structure);

		if (!sink->window)
			return;

		/* We calculate scaling using the original video frames geometry to include
		pixel aspect ratio scaling. */
		xscale = (gdouble)sink->video_width / disp_rect->width;
		yscale = (gdouble)sink->video_height / disp_rect->height;

		/* Converting pointer coordinates to the non scaled geometry */
		if (gst_structure_get_double(structure, "pointer_x", &x)) {
			x = MIN(x, disp_rect->x + disp_rect->width);
			x = MAX(x - disp_rect->x, 0);
			gst_structure_set(structure, "pointer_x", G_TYPE_DOUBLE,
				(gdouble)x * xscale, NULL);
		}
		if (gst_structure_get_double(structure, "pointer_y", &y)) {
			y = MIN(y, disp_rect->y + disp_rect->height);
			y = MAX(y - disp_rect->y, 0);
			gst_structure_set(structure, "pointer_y", G_TYPE_DOUBLE,
				(gdouble)y * yscale, NULL);
		}

		gst_pad_send_event(peer, event);
		gst_object_unref(peer);
	}
}

static void
gst_mfxsink_navigation_iface_init(GstNavigationInterface * iface)
{
	iface->send_event = gst_mfxsink_navigation_send_event;
}

/* ------------------------------------------------------------------------ */
/* --- Common implementation                                            --- */
/* ------------------------------------------------------------------------ */

static gboolean
gst_mfxsink_reconfigure_window(GstMfxSink * sink)
{
	guint win_width, win_height;

	gst_mfx_window_reconfigure(sink->window);
	gst_mfx_window_get_size(sink->window, &win_width, &win_height);
	if (win_width != sink->window_width || win_height != sink->window_height) {
		if (!gst_mfxsink_ensure_render_rect(sink, win_width, win_height))
			return FALSE;
		GST_INFO("window was resized from %ux%u to %ux%u",
			sink->window_width, sink->window_height, win_width, win_height);
		sink->window_width = win_width;
		sink->window_height = win_height;
		return TRUE;
	}
	return FALSE;
}

static gpointer
gst_mfxsink_event_thread(GstMfxSink * sink)
{
	GST_OBJECT_LOCK(sink);
	while (!sink->event_thread_cancel) {
		GST_OBJECT_UNLOCK(sink);
		sink->backend->handle_events(sink);
		g_usleep(G_USEC_PER_SEC / 20);
		GST_OBJECT_LOCK(sink);
	}
	GST_OBJECT_UNLOCK(sink);
	return NULL;
}

static void
gst_mfxsink_set_event_handling(GstMfxSink * sink, gboolean handle_events)
{
	GThread *thread = NULL;

	if (!sink->backend || !sink->backend->event_thread_needed)
		return;

	GST_OBJECT_LOCK(sink);
	sink->handle_events = handle_events;
	if (handle_events && !sink->event_thread) {
		/* Setup our event listening thread */
		GST_DEBUG("starting xevent thread");
		if (sink->backend->pre_start_event_thread)
			sink->backend->pre_start_event_thread(sink);

		sink->event_thread_cancel = FALSE;
		sink->event_thread = g_thread_try_new("mfxsink-events",
			(GThreadFunc)gst_mfxsink_event_thread, sink, NULL);
	}
	else if (!handle_events && sink->event_thread) {
		GST_DEBUG("stopping xevent thread");
		if (sink->backend->pre_stop_event_thread)
			sink->backend->pre_stop_event_thread(sink);

		/* Grab thread and mark it as NULL */
		thread = sink->event_thread;
		sink->event_thread = NULL;
		sink->event_thread_cancel = TRUE;
	}
	GST_OBJECT_UNLOCK(sink);

	/* Wait for our event thread to finish */
	if (thread) {
		g_thread_join(thread);
		GST_DEBUG("xevent thread stopped");
	}
}

static void
gst_mfxsink_ensure_backend(GstMfxSink * sink)
{
    GstMfxPluginBase *plugin = GST_MFX_PLUGIN_BASE(sink);

	switch (plugin->display_type_req) {
#if USE_EGL
	case GST_MFX_DISPLAY_TYPE_EGL:
        sink->backend = gst_mfxsink_backend_egl();
        break;
#endif
#if USE_WAYLAND
	//case GST_MFX_DISPLAY_TYPE_WAYLAND:
		//sink->backend = gst_mfxsink_backend_wayland();
		//break;
#endif
	default:
		GST_ERROR("failed to initialize GstMfxSink backend");
		g_assert_not_reached();
		break;
	}
}

static gboolean
gst_mfxsink_ensure_render_rect(GstMfxSink * sink, guint width,
	guint height)
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

		GST_DEBUG("force-aspect-ratio is false; distorting while scaling video");
		GST_DEBUG("render rect (%d,%d):%ux%u",
			display_rect->x, display_rect->y,
			display_rect->width, display_rect->height);
		return TRUE;
	}

	GST_DEBUG("ensure render rect within %ux%u bounds", width, height);

	gst_mfx_display_get_pixel_aspect_ratio(GST_MFX_PLUGIN_BASE_DISPLAY
		(sink), &display_par_n, &display_par_d);
	GST_DEBUG("display pixel-aspect-ratio %d/%d", display_par_n, display_par_d);

	success = gst_video_calculate_display_ratio(&num, &den,
		sink->video_width, sink->video_height,
		sink->video_par_n, sink->video_par_d, display_par_n, display_par_d);
	if (!success)
		return FALSE;
	GST_DEBUG("video size %dx%d, calculated ratio %d/%d",
		sink->video_width, sink->video_height, num, den);

	display_rect->width = gst_util_uint64_scale_int(height, num, den);
	if (display_rect->width <= width) {
		GST_DEBUG("keeping window height");
		display_rect->height = height;
	}
	else {
		GST_DEBUG("keeping window width");
		display_rect->width = width;
		display_rect->height = gst_util_uint64_scale_int(width, den, num);
	}
	GST_DEBUG("scaling video to %ux%u", display_rect->width,
		display_rect->height);

	g_assert(display_rect->width <= width);
	g_assert(display_rect->height <= height);

	display_rect->x = (width - display_rect->width) / 2;
	display_rect->y = (height - display_rect->height) / 2;

	GST_DEBUG("render rect (%d,%d):%ux%u",
		display_rect->x, display_rect->y,
		display_rect->width, display_rect->height);
	return TRUE;
}

static inline gboolean
gst_mfxsink_ensure_window(GstMfxSink * sink, guint width, guint height)
{
	return sink->window || sink->backend->create_window(sink, width, height);
}

static void
gst_mfxsink_ensure_window_size(GstMfxSink * sink, guint * width_ptr,
guint * height_ptr)
{
	GstMfxDisplay *const display = GST_MFX_PLUGIN_BASE_DISPLAY(sink);
	GstVideoRectangle src_rect, dst_rect, out_rect;
	guint num, den, display_width, display_height, display_par_n, display_par_d;
	gboolean success, scale;

	gst_mfx_display_get_size(display, &display_width, &display_height);
	if (sink->fullscreen) {
		*width_ptr = display_width;
		*height_ptr = display_height;
		return;
	}

	gst_mfx_display_get_pixel_aspect_ratio(display,
		&display_par_n, &display_par_d);

	success = gst_video_calculate_display_ratio(&num, &den,
		sink->video_width, sink->video_height,
		sink->video_par_n, sink->video_par_d, display_par_n, display_par_d);
	if (!success) {
		num = sink->video_par_n;
		den = sink->video_par_d;
	}

	src_rect.x = 0;
	src_rect.y = 0;
	src_rect.w = gst_util_uint64_scale_int(sink->video_height, num, den);
	src_rect.h = sink->video_height;
	dst_rect.x = 0;
	dst_rect.y = 0;
	dst_rect.w = display_width;
	dst_rect.h = display_height;
	scale = (src_rect.w > dst_rect.w || src_rect.h > dst_rect.h);
	gst_video_sink_center_rect(src_rect, dst_rect, &out_rect, scale);
	*width_ptr = out_rect.w;
	*height_ptr = out_rect.h;
}

static const gchar *
get_display_type_name(GstMfxDisplayType display_type)
{
	gpointer const klass = g_type_class_peek(GST_MFX_TYPE_DISPLAY_TYPE);
	GEnumValue *const e = g_enum_get_value(klass, display_type);

	if (e)
		return e->value_name;
	return "<unknown-type>";
}

static void
gst_mfxsink_display_changed(GstMfxPluginBase * plugin)
{
	GstMfxSink *const sink = GST_MFXSINK_CAST(plugin);

	GST_INFO("created %s %p", get_display_type_name(plugin->display_type),
		plugin->display);

	gst_mfxsink_ensure_backend(sink);
}

static gboolean
gst_mfxsink_start(GstBaseSink * base_sink)
{
	GstMfxSink *const sink = GST_MFXSINK_CAST(base_sink);

	return gst_mfx_plugin_base_open(GST_MFX_PLUGIN_BASE(sink));
}

static gboolean
gst_mfxsink_stop(GstBaseSink * base_sink)
{
	GstMfxSink *const sink = GST_MFXSINK_CAST(base_sink);

	gst_buffer_replace(&sink->video_buffer, NULL);
	gst_mfx_window_replace(&sink->window, NULL);

	gst_mfx_plugin_base_close(GST_MFX_PLUGIN_BASE(sink));
	return TRUE;
}

static GstCaps *
gst_mfxsink_get_caps_impl(GstBaseSink * base_sink)
{
	GstMfxSink *const sink = GST_MFXSINK_CAST(base_sink);
	GstCaps *out_caps, *raw_caps;

	out_caps = gst_static_pad_template_get_caps(&gst_mfxsink_sink_factory);
	if (!out_caps)
		return NULL;

	/*if (GST_MFX_PLUGIN_BASE_DISPLAY(sink)) {
		raw_caps = gst_mfx_plugin_base_get_allowed_raw_caps(GST_MFX_PLUGIN_BASE(sink));
		if (raw_caps) {
			out_caps = gst_caps_make_writable(out_caps);
			gst_caps_append(out_caps, gst_caps_copy(raw_caps));
		}
	}*/
	return out_caps;
}

static inline GstCaps *
gst_mfxsink_get_caps(GstBaseSink * base_sink, GstCaps * filter)
{
	GstCaps *caps, *out_caps;

	caps = gst_mfxsink_get_caps_impl(base_sink);
	if (caps && filter) {
		out_caps = gst_caps_intersect_full(caps, filter, GST_CAPS_INTERSECT_FIRST);
		gst_caps_unref(caps);
	}
	else
		out_caps = caps;
	return out_caps;
}

static gboolean
gst_mfxsink_set_caps(GstBaseSink * base_sink, GstCaps * caps)
{
	GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE(base_sink);
	GstMfxSink *const sink = GST_MFXSINK_CAST(base_sink);
	GstVideoInfo *const vip = GST_MFX_PLUGIN_BASE_SINK_PAD_INFO(sink);
	guint win_width, win_height;

	if (!gst_mfxsink_ensure_display(sink))
		return FALSE;

#if USE_EGL
	if (plugin->display_type_req == GST_MFX_DISPLAY_TYPE_EGL &&
        GST_MFX_PLUGIN_BASE_DISPLAY_TYPE(sink) != GST_MFX_DISPLAY_TYPE_EGL) {
        GstMfxDisplay *egl_display;

        egl_display = gst_mfx_display_egl_new (NULL, 2);
        if (!egl_display)
            return FALSE;

        gst_mfx_display_replace(&GST_MFX_PLUGIN_BASE_DISPLAY(sink), egl_display);
        gst_mfx_display_unref(egl_display);

        GST_MFX_PLUGIN_BASE_DISPLAY_TYPE(sink) = GST_MFX_DISPLAY_TYPE_EGL;
	}
#endif

	if (GST_MFX_PLUGIN_BASE_DISPLAY_TYPE(sink) == GST_MFX_DISPLAY_TYPE_DRM)
		return TRUE;

	if (!gst_mfx_plugin_base_set_caps(plugin, caps, NULL))
		return FALSE;

	sink->video_width = GST_VIDEO_INFO_WIDTH(vip);
	sink->video_height = GST_VIDEO_INFO_HEIGHT(vip);
	sink->video_par_n = GST_VIDEO_INFO_PAR_N(vip);
	sink->video_par_d = GST_VIDEO_INFO_PAR_D(vip);
	GST_DEBUG("video pixel-aspect-ratio %d/%d",
		sink->video_par_n, sink->video_par_d);

	gst_caps_replace(&sink->caps, caps);

	gst_mfxsink_ensure_window_size(sink, &win_width, &win_height);
	if (sink->window) {
		if (sink->fullscreen)
			gst_mfx_window_set_size(sink->window, win_width, win_height);
	}
	else {
		if (sink->window)
			return TRUE;
		if (!gst_mfxsink_ensure_window(sink, win_width, win_height))
			return FALSE;
		gst_mfx_window_set_fullscreen(sink->window, sink->fullscreen);
		gst_mfx_window_show(sink->window);
		gst_mfx_window_get_size(sink->window, &win_width, &win_height);
		gst_mfxsink_set_event_handling(sink, sink->handle_events);
	}
	sink->window_width = win_width;
	sink->window_height = win_height;
	GST_DEBUG("window size %ux%u", win_width, win_height);

	return gst_mfxsink_ensure_render_rect(sink, win_width, win_height);
}

static GstFlowReturn
gst_mfxsink_show_frame(GstVideoSink * video_sink, GstBuffer * src_buffer)
{
    GstMfxSink *const sink = GST_MFXSINK_CAST(video_sink);
	GstMfxVideoMeta *meta;
	GstMfxSurfaceProxy *proxy;
	GstMfxSurface *surface;
	GstBuffer *buffer;
	guint flags;
	GstMfxRectangle *surface_rect = NULL;
	GstMfxRectangle tmp_rect;
	GstFlowReturn ret;

	GstVideoCropMeta *const crop_meta =
		gst_buffer_get_video_crop_meta(src_buffer);
	if (crop_meta) {
		surface_rect = &tmp_rect;
		surface_rect->x = crop_meta->x;
		surface_rect->y = crop_meta->y;
		surface_rect->width = crop_meta->width;
		surface_rect->height = crop_meta->height;
	}

	ret = gst_mfx_plugin_base_get_input_buffer(GST_MFX_PLUGIN_BASE(sink),
		src_buffer, &buffer);
	if (ret != GST_FLOW_OK && ret != GST_FLOW_NOT_SUPPORTED)
		return ret;

	meta = gst_buffer_get_mfx_video_meta(buffer);
	GST_MFX_PLUGIN_BASE_DISPLAY_REPLACE(sink,
		gst_mfx_video_meta_get_display(meta));

	proxy = gst_mfx_video_meta_get_surface_proxy(meta);
	if (!proxy)
		goto no_surface;

	surface = gst_mfx_video_meta_get_surface(meta);
	if (!surface)
		goto no_surface;


	GST_DEBUG("render surface %" GST_MFX_ID_FORMAT,
		GST_MFX_ID_ARGS(gst_mfx_surface_get_id(surface)));

	if (!surface_rect)
		surface_rect = (GstMfxRectangle *)
            gst_mfx_video_meta_get_render_rect(meta);

	if (surface_rect)
		GST_DEBUG("render rect (%d,%d), size %ux%u",
            surface_rect->x, surface_rect->y,
            surface_rect->width, surface_rect->height);

	if (!sink->backend->render_surface(sink, surface, surface_rect))
		goto error;

	if (sink->signal_handoffs)
		g_signal_emit(sink, gst_mfxsink_signals[HANDOFF_SIGNAL], 0, buffer);

	/* Retain VA surface until the next one is displayed */
	/* Need to release the lock for the duration, otherwise a deadlock is possible */
	//gst_mfx_display_unlock(GST_MFX_PLUGIN_BASE_DISPLAY(sink));
	gst_buffer_replace(&sink->video_buffer, buffer);
	//gst_mfx_display_lock(GST_MFX_PLUGIN_BASE_DISPLAY(sink));

	ret = GST_FLOW_OK;

done:
	gst_buffer_unref(buffer);
	return ret;

error:
	GST_ELEMENT_ERROR(sink, RESOURCE, WRITE,
		("Internal error: could not render surface"), (NULL));
	ret = GST_FLOW_ERROR;
	goto done;

no_surface:
	/* No surface or surface proxy. That's very bad! */
	GST_WARNING_OBJECT(sink, "could not get surface");
	ret = GST_FLOW_ERROR;
	goto done;
}

static gboolean
gst_mfxsink_propose_allocation(GstBaseSink * base_sink, GstQuery * query)
{
	GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE(base_sink);

	if (!gst_mfx_plugin_base_propose_allocation(plugin, query))
		return FALSE;

	gst_query_add_allocation_meta(query, GST_VIDEO_CROP_META_API_TYPE, NULL);
	return TRUE;
}

static gboolean
gst_mfxsink_query(GstBaseSink * base_sink, GstQuery * query)
{
	GstMfxSink *const sink = GST_MFXSINK_CAST(base_sink);
	GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE(sink);
	gboolean ret = FALSE;

	switch (GST_QUERY_TYPE(query)) {
	case GST_QUERY_CONTEXT:
		ret = gst_mfx_handle_context_query(query, plugin->display);
		break;
	default:
		ret = GST_BASE_SINK_CLASS(gst_mfxsink_parent_class)->query(base_sink,
			query);
		break;
	}

	return ret;
}

static void
gst_mfxsink_destroy(GstMfxSink * sink)
{
	gst_mfxsink_set_event_handling(sink, FALSE);

	gst_buffer_replace(&sink->video_buffer, NULL);
	gst_caps_replace(&sink->caps, NULL);
}

static void
gst_mfxsink_finalize(GObject * object)
{
	gst_mfxsink_destroy(GST_MFXSINK_CAST(object));

	gst_mfx_plugin_base_finalize(GST_MFX_PLUGIN_BASE(object));
	G_OBJECT_CLASS(gst_mfxsink_parent_class)->finalize(object);
}

static void
gst_mfxsink_set_property(GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
	GstMfxSink *const sink = GST_MFXSINK_CAST(object);

	switch (prop_id) {
	case PROP_DISPLAY_TYPE:
		gst_mfx_plugin_base_set_display_type(GST_MFX_PLUGIN_BASE(sink),
			g_value_get_enum(value));
		break;
	case PROP_DISPLAY_NAME:
		gst_mfx_plugin_base_set_display_name(GST_MFX_PLUGIN_BASE(sink),
			g_value_get_string(value));
		break;
	case PROP_FULLSCREEN:
		sink->fullscreen = g_value_get_boolean(value);
		break;
	case PROP_FORCE_ASPECT_RATIO:
		sink->keep_aspect = g_value_get_boolean(value);
		break;
	case PROP_SIGNAL_HANDOFFS:
		sink->signal_handoffs = g_value_get_boolean(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
gst_mfxsink_get_property(GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
	GstMfxSink *const sink = GST_MFXSINK_CAST(object);

	switch (prop_id) {
	case PROP_DISPLAY_TYPE:
		g_value_set_enum(value, GST_MFX_PLUGIN_BASE_DISPLAY_TYPE(sink));
		break;
	case PROP_DISPLAY_NAME:
		g_value_set_string(value, GST_MFX_PLUGIN_BASE_DISPLAY_NAME(sink));
		break;
	case PROP_FULLSCREEN:
		g_value_set_boolean(value, sink->fullscreen);
		break;
	case PROP_FORCE_ASPECT_RATIO:
		g_value_set_boolean(value, sink->keep_aspect);
		break;
	case PROP_SIGNAL_HANDOFFS:
		g_value_set_boolean(value, sink->signal_handoffs);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static gboolean
gst_mfxsink_unlock(GstBaseSink * base_sink)
{
	GstMfxSink *const sink = GST_MFXSINK_CAST(base_sink);

	if (sink->window)
		return gst_mfx_window_unblock(sink->window);

	return TRUE;
}

static gboolean
gst_mfxsink_unlock_stop(GstBaseSink * base_sink)
{
	GstMfxSink *const sink = GST_MFXSINK_CAST(base_sink);

	if (sink->window)
		return gst_mfx_window_unblock_cancel(sink->window);

	return TRUE;
}

static void
gst_mfxsink_set_bus(GstElement * element, GstBus * bus)
{
	/* Make sure to allocate a VA display in the sink element first,
	so that upstream elements could query a display that was
	allocated here, and that exactly matches what the user
	requested through the "display" property */
	if (!GST_ELEMENT_BUS(element) && bus)
		gst_mfxsink_ensure_display(GST_MFXSINK_CAST(element));

	GST_ELEMENT_CLASS(gst_mfxsink_parent_class)->set_bus(element, bus);
}

static void
gst_mfxsink_class_init(GstMfxSinkClass * klass)
{
	GObjectClass *const object_class = G_OBJECT_CLASS(klass);
	GstElementClass *const element_class = GST_ELEMENT_CLASS(klass);
	GstBaseSinkClass *const basesink_class = GST_BASE_SINK_CLASS(klass);
	GstVideoSinkClass *const videosink_class = GST_VIDEO_SINK_CLASS(klass);
	GstMfxPluginBaseClass *const base_plugin_class =
		GST_MFX_PLUGIN_BASE_CLASS(klass);
	GstPadTemplate *pad_template;

	GST_DEBUG_CATEGORY_INIT(gst_debug_mfxsink,
		GST_PLUGIN_NAME, 0, GST_PLUGIN_DESC);

	gst_mfx_plugin_base_class_init(base_plugin_class);
	base_plugin_class->display_changed = gst_mfxsink_display_changed;

	object_class->finalize = gst_mfxsink_finalize;
	object_class->set_property = gst_mfxsink_set_property;
	object_class->get_property = gst_mfxsink_get_property;

	basesink_class->start = gst_mfxsink_start;
	basesink_class->stop = gst_mfxsink_stop;
	basesink_class->get_caps = gst_mfxsink_get_caps;
	basesink_class->set_caps = gst_mfxsink_set_caps;
	basesink_class->query = GST_DEBUG_FUNCPTR(gst_mfxsink_query);
	basesink_class->propose_allocation = gst_mfxsink_propose_allocation;
	basesink_class->unlock = gst_mfxsink_unlock;
	basesink_class->unlock_stop = gst_mfxsink_unlock_stop;

	videosink_class->show_frame = GST_DEBUG_FUNCPTR(gst_mfxsink_show_frame);

	element_class->set_bus = gst_mfxsink_set_bus;
	gst_element_class_set_static_metadata(element_class,
		"MFX sink", "Sink/Video", GST_PLUGIN_DESC,
		"Ishmael Sameen <ishmael.visayana.sameen@intel.com>");

	pad_template = gst_static_pad_template_get(&gst_mfxsink_sink_factory);
	gst_element_class_add_pad_template(element_class, pad_template);

	/**
	* GstMfxSink:display:
	*
	* The type of display to use.
	*/
	g_properties[PROP_DISPLAY_TYPE] =
		g_param_spec_enum("display",
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
		g_param_spec_string("display-name",
		"display name",
		"display name to use", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	/**
	* GstMfxSink:fullscreen:
	*
	* Selects whether fullscreen mode is enabled or not.
	*/
	g_properties[PROP_FULLSCREEN] =
		g_param_spec_boolean("fullscreen",
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
		g_param_spec_boolean("force-aspect-ratio",
		"Force aspect ratio",
		"When enabled, scaling will respect original aspect ratio",
		TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	/**
	* GstMfxSink:signal-handoffs:
	*
	* Send a signal after rendering the buffer.
	*/
	g_properties[PROP_SIGNAL_HANDOFFS] =
		g_param_spec_boolean("signal-handoffs", "Signal handoffs",
		"Send a signal after rendering the buffer", DEFAULT_SIGNAL_HANDOFFS,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(object_class, N_PROPERTIES, g_properties);

	/**
	* GstMfxSink::handoff:
	* @object: the #GstMfxSink instance
	* @buffer: the buffer that was rendered
	*
	* This signal gets emitted after rendering the frame.
	*/
	gst_mfxsink_signals[HANDOFF_SIGNAL] =
		g_signal_new("handoff", G_TYPE_FROM_CLASS(klass),
		G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_generic,
		G_TYPE_NONE, 1, GST_TYPE_BUFFER | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
gst_mfxsink_init(GstMfxSink * sink)
{
	GstMfxPluginBase *const plugin = GST_MFX_PLUGIN_BASE(sink);

	gst_mfx_plugin_base_init(plugin, GST_CAT_DEFAULT);
	gst_mfx_plugin_base_set_display_type(plugin, DEFAULT_DISPLAY_TYPE);

	sink->video_par_n = 1;
	sink->video_par_d = 1;
	sink->handle_events = TRUE;
	sink->keep_aspect = TRUE;
	sink->signal_handoffs = DEFAULT_SIGNAL_HANDOFFS;
	gst_video_info_init(&sink->video_info);
}
