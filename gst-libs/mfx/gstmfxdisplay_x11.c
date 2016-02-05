#include "sysdeps.h"
#include <string.h>
#include "gstmfxdisplay_priv.h"
#include "gstmfxdisplay_x11.h"
#include "gstmfxdisplay_x11_priv.h"
#include "gstmfxwindow_x11.h"

#ifdef HAVE_XRANDR
# include <X11/extensions/Xrandr.h>
#endif

#define DEBUG 1
#include "gstmfxdebug.h"

static const guint g_display_types = 1U << GST_MFX_DISPLAY_TYPE_X11;

static inline const gchar *
get_default_display_name(void)
{
	static const gchar *g_display_name;

	if (!g_display_name)
		g_display_name = getenv("DISPLAY");
	return g_display_name;
}

/* Reconstruct a display name without our prefix */
static const gchar *
get_display_name(GstMfxDisplayX11 * display)
{
	GstMfxDisplayX11Private *const priv = &display->priv;
	const gchar *display_name = priv->display_name;

	if (!display_name || *display_name == '\0')
		return NULL;
	return display_name;
}

/* Mangle display name with our prefix */
static gboolean
set_display_name(GstMfxDisplayX11 * display, const gchar * display_name)
{
	GstMfxDisplayX11Private *const priv = &display->priv;

	g_free(priv->display_name);

	if (!display_name) {
		display_name = get_default_display_name();
		if (!display_name)
			display_name = "";
	}
	priv->display_name = g_strdup(display_name);
	return priv->display_name != NULL;
}

/* Set synchronous behavious on the underlying X11 display */
static void
set_synchronous(GstMfxDisplayX11 * display, gboolean synchronous)
{
	GstMfxDisplayX11Private *const priv = &display->priv;

	if (priv->synchronous != synchronous) {
		priv->synchronous = synchronous;
		if (priv->x11_display) {
			GST_MFX_DISPLAY_LOCK(display);
			XSynchronize(priv->x11_display, synchronous);
			GST_MFX_DISPLAY_UNLOCK(display);
		}
	}
}

/* Check for display server extensions */
static void
check_extensions(GstMfxDisplayX11 * display)
{
	GstMfxDisplayX11Private *const priv =
		GST_MFX_DISPLAY_X11_PRIVATE(display);
	int evt_base, err_base;

#ifdef HAVE_XRANDR
	priv->use_xrandr = XRRQueryExtension(priv->x11_display,
		&evt_base, &err_base);
#endif
}

static gboolean
gst_mfx_display_x11_bind_display(GstMfxDisplay * base_display,
	gpointer native_display)
{
	GstMfxDisplayX11 *const display = GST_MFX_DISPLAY_X11_CAST(base_display);
	GstMfxDisplayX11Private *const priv = &display->priv;

	priv->x11_display = native_display;
	priv->x11_screen = DefaultScreen(native_display);
	priv->use_foreign_display = TRUE;

	check_extensions(display);

	if (!set_display_name(display, XDisplayString(priv->x11_display)))
		return FALSE;
	return TRUE;
}

static gboolean
gst_mfx_display_x11_open_display(GstMfxDisplay * base_display,
    const gchar * name)
{
	GstMfxDisplayX11 *const display = GST_MFX_DISPLAY_X11_CAST(base_display);
	GstMfxDisplayX11Private *const priv = &display->priv;

	if (!set_display_name(display, name))
		return FALSE;

    priv->x11_display = XOpenDisplay (get_display_name (display));
    if (!priv->x11_display)
        return FALSE;
    priv->use_foreign_display = FALSE;

    priv->x11_screen = DefaultScreen (priv->x11_display);

	check_extensions(display);
	return TRUE;
}

static void
gst_mfx_display_x11_close_display(GstMfxDisplay * display)
{
	GstMfxDisplayX11Private *const priv =
		GST_MFX_DISPLAY_X11_PRIVATE(display);

	if (priv->x11_display) {
		if (!priv->use_foreign_display)
			XCloseDisplay(priv->x11_display);
		priv->x11_display = NULL;
	}

	if (priv->display_name) {
		g_free(priv->display_name);
		priv->display_name = NULL;
	}
}

static void
gst_mfx_display_x11_sync(GstMfxDisplay * display)
{
	GstMfxDisplayX11Private *const priv =
		GST_MFX_DISPLAY_X11_PRIVATE(display);

	if (priv->x11_display) {
		GST_MFX_DISPLAY_LOCK(display);
		XSync(priv->x11_display, False);
		GST_MFX_DISPLAY_UNLOCK(display);
	}
}

static void
gst_mfx_display_x11_flush(GstMfxDisplay * display)
{
	GstMfxDisplayX11Private *const priv =
		GST_MFX_DISPLAY_X11_PRIVATE(display);

	if (priv->x11_display) {
		GST_MFX_DISPLAY_LOCK(display);
		XFlush(priv->x11_display);
		GST_MFX_DISPLAY_UNLOCK(display);
	}
}

static gboolean
gst_mfx_display_x11_get_display_info(GstMfxDisplay * display,
	GstMfxDisplayInfo * info)
{
	GstMfxDisplayX11Private *const priv =
		GST_MFX_DISPLAY_X11_PRIVATE(display);

	/* Otherwise, create VA display if there is none already */
	info->native_display = priv->x11_display;
	info->display_name = priv->display_name;
	info->display_type = GST_MFX_DISPLAY_TYPE_X11;
	return TRUE;
}

static void
gst_mfx_display_x11_get_size(GstMfxDisplay * display,
	guint * pwidth, guint * pheight)
{
	GstMfxDisplayX11Private *const priv =
		GST_MFX_DISPLAY_X11_PRIVATE(display);

	if (!priv->x11_display)
		return;

	if (pwidth)
		*pwidth = DisplayWidth(priv->x11_display, priv->x11_screen);

	if (pheight)
		*pheight = DisplayHeight(priv->x11_display, priv->x11_screen);
}

static void
gst_mfx_display_x11_get_size_mm(GstMfxDisplay * display,
    guint * pwidth, guint * pheight)
{
	GstMfxDisplayX11Private *const priv =
		GST_MFX_DISPLAY_X11_PRIVATE(display);
	guint width_mm, height_mm;

	if (!priv->x11_display)
		return;

	width_mm = DisplayWidthMM(priv->x11_display, priv->x11_screen);
	height_mm = DisplayHeightMM(priv->x11_display, priv->x11_screen);

#ifdef HAVE_XRANDR
	/* XXX: fix up physical size if the display is rotated */
	if (priv->use_xrandr) {
		XRRScreenConfiguration *xrr_config = NULL;
		XRRScreenSize *xrr_sizes;
		Window win;
		int num_xrr_sizes, size_id, screen;
		Rotation rotation;

		do {
			win = DefaultRootWindow(priv->x11_display);
			screen = XRRRootToScreen(priv->x11_display, win);

			xrr_config = XRRGetScreenInfo(priv->x11_display, win);
			if (!xrr_config)
				break;

			size_id = XRRConfigCurrentConfiguration(xrr_config, &rotation);
			if (rotation == RR_Rotate_0 || rotation == RR_Rotate_180)
				break;

			xrr_sizes = XRRSizes(priv->x11_display, screen, &num_xrr_sizes);
			if (!xrr_sizes || size_id >= num_xrr_sizes)
				break;

			width_mm = xrr_sizes[size_id].mheight;
			height_mm = xrr_sizes[size_id].mwidth;
		} while (0);
		if (xrr_config)
			XRRFreeScreenConfigInfo(xrr_config);
	}
#endif

	if (pwidth)
		*pwidth = width_mm;

	if (pheight)
		*pheight = height_mm;
}

static GstMfxWindow *
gst_mfx_display_x11_create_window(GstMfxDisplay * display,
	guint width, guint height)
{
	return gst_mfx_window_x11_new(display, width, height);
}

void
gst_mfx_display_x11_class_init(GstMfxDisplayX11Class * klass)
{
	GstMfxMiniObjectClass *const object_class =
		GST_MFX_MINI_OBJECT_CLASS(klass);
	GstMfxDisplayClass *const dpy_class = GST_MFX_DISPLAY_CLASS(klass);

	gst_mfx_display_class_init(&klass->parent_class);

	object_class->size = sizeof (GstMfxDisplayX11);
	dpy_class->display_type = GST_MFX_DISPLAY_TYPE_X11;
	dpy_class->bind_display = gst_mfx_display_x11_bind_display;
	dpy_class->open_display = gst_mfx_display_x11_open_display;
	dpy_class->close_display = gst_mfx_display_x11_close_display;
	dpy_class->sync = gst_mfx_display_x11_sync;
	dpy_class->flush = gst_mfx_display_x11_flush;
	dpy_class->get_display = gst_mfx_display_x11_get_display_info;
	dpy_class->get_size = gst_mfx_display_x11_get_size;
	dpy_class->get_size_mm = gst_mfx_display_x11_get_size_mm;
	dpy_class->create_window = gst_mfx_display_x11_create_window;
}

static inline const GstMfxDisplayClass *
gst_mfx_display_x11_class(void)
{
	static GstMfxDisplayX11Class g_class;
	static gsize g_class_init = FALSE;

	if (g_once_init_enter(&g_class_init)) {
		gst_mfx_display_x11_class_init(&g_class);
		g_once_init_leave(&g_class_init, TRUE);
	}
	return GST_MFX_DISPLAY_CLASS(&g_class);
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
gst_mfx_display_x11_new(const gchar * display_name)
{
	return gst_mfx_display_new(gst_mfx_display_x11_class(),
		GST_MFX_DISPLAY_INIT_FROM_DISPLAY_NAME, (gpointer)display_name);
}

/**
* gst_mfx_display_x11_new_with_display:
* @x11_display: an X11 #Display
*
* Creates a #GstMfxDisplay based on the X11 @x11_display
* display. The caller still owns the display and must call
* XCloseDisplay() when all #GstMfxDisplay references are
* released. Doing so too early can yield undefined behaviour.
*
* Return value: a newly allocated #GstMfxDisplay object
*/
GstMfxDisplay *
gst_mfx_display_x11_new_with_display(Display * x11_display)
{
	g_return_val_if_fail(x11_display, NULL);

	return gst_mfx_display_new(gst_mfx_display_x11_class(),
		GST_MFX_DISPLAY_INIT_FROM_NATIVE_DISPLAY, x11_display);
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
gst_mfx_display_x11_get_display(GstMfxDisplayX11 * display)
{
	g_return_val_if_fail(GST_MFX_IS_DISPLAY_X11(display), NULL);

	return GST_MFX_DISPLAY_XDISPLAY(display);
}

/**
* gst_mfx_display_x11_get_screen:
* @display: a #GstMfxDisplayX11
*
* Returns the default X11 screen that was created by
* gst_mfx_display_x11_new() or that was bound from
* gst_mfx_display_x11_new_with_display().
*
* Return value: the X11 #Display attached to @display
*/
int
gst_mfx_display_x11_get_screen(GstMfxDisplayX11 * display)
{
	g_return_val_if_fail(GST_MFX_IS_DISPLAY_X11(display), -1);

	return GST_MFX_DISPLAY_XSCREEN(display);
}

/**
* gst_mfx_display_x11_set_synchronous:
* @display: a #GstMfxDisplayX11
* @synchronous: boolean value that indicates whether to enable or
*   disable synchronization
*
* If @synchronous is %TRUE, gst_mfx_display_x11_set_synchronous()
* turns on synchronous behaviour on the underlying X11
* display. Otherwise, synchronous behaviour is disabled if
* @synchronous is %FALSE.
*/
void
gst_mfx_display_x11_set_synchronous(GstMfxDisplayX11 * display,
gboolean synchronous)
{
	g_return_if_fail(GST_MFX_IS_DISPLAY_X11(display));

	set_synchronous(display, synchronous);
}
