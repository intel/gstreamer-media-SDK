#include "sysdeps.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <drm.h>
#include <xf86drm.h>
#include <intel_bufmgr.h>
#include "gstmfxdisplay_priv.h"
#include "gstmfxdisplay_wayland.h"
#include "gstmfxdisplay_wayland_priv.h"
#include "gstmfxwindow_wayland.h"

#define DEBUG 1
#include "gstmfxdebug.h"

static const guint g_display_types = 1U << GST_MFX_DISPLAY_TYPE_WAYLAND;

static inline const gchar *
get_default_display_name(void)
{
	static const gchar *g_display_name;

	if (!g_display_name)
		g_display_name = getenv("WAYLAND_DISPLAY");
	return g_display_name;
}

/* Mangle display name with our prefix */
static gboolean
set_display_name(GstMfxDisplay * display, const gchar * display_name)
{
	GstMfxDisplayWaylandPrivate *const priv =
		GST_MFX_DISPLAY_WAYLAND_GET_PRIVATE(display);

	g_free(priv->display_name);

	if (!display_name) {
		display_name = get_default_display_name();
		if (!display_name)
			display_name = "";
	}
	priv->display_name = g_strdup(display_name);
	return priv->display_name != NULL;
}

static void
output_handle_geometry(void *data, struct wl_output *output,
    int x, int y, int physical_width, int physical_height,
    int subpixel, const char *make, const char *model, int transform)
{
	GstMfxDisplayWaylandPrivate *const priv = data;

	priv->phys_width = physical_width;
	priv->phys_height = physical_height;
}

static void
output_handle_mode(void *data, struct wl_output *wl_output,
    uint32_t flags, int width, int height, int refresh)
{
	GstMfxDisplayWaylandPrivate *const priv = data;

	if (flags & WL_OUTPUT_MODE_CURRENT) {
		priv->width = width;
		priv->height = height;
	}
}

static const struct wl_output_listener output_listener = {
	output_handle_geometry,
	output_handle_mode,
};

/* DRM listeners for wl_drm interface */
static void drm_handle_device(void *data
    , struct wl_drm *drm
    , const char *device)
{
	GstMfxDisplayWaylandPrivate *const priv = data;
	priv->drm_device_name = g_strdup(device);
    if (!priv->drm_device_name)
        return;

    drm_magic_t magic;
    priv->drm_fd = open(priv->drm_device_name, O_RDWR | O_CLOEXEC);
    if (-1 == priv->drm_fd) {
        g_printf("Error: Could not open %s\n",priv->drm_device_name);
        return;
    }
    drmGetMagic(priv->drm_fd, &magic);
    wl_drm_authenticate(priv->drm, magic);
}

static void drm_handle_format(void *data
    , struct wl_drm *drm
    , uint32_t format)
{
    /* NOT IMPLEMENTED */
}

static void drm_handle_capabilities(void *data
    , struct wl_drm *drm
    , uint32_t value)
{
    /* NOT IMPLEMENTED */
}

static void drm_handle_authenticated(void *data
    , struct wl_drm *drm)
{
	GstMfxDisplayWaylandPrivate *const priv = data;
	priv->bufmgr = drm_intel_bufmgr_gem_init(priv->drm_fd, BATCH_SIZE);
	priv->is_auth = TRUE;
}


static const struct wl_drm_listener drm_listener = {
	drm_handle_device,
	drm_handle_format,
	drm_handle_authenticated,
	drm_handle_capabilities
};

static void
registry_handle_global(void *data,
struct wl_registry *registry,
	uint32_t id, const char *interface, uint32_t version)
{
	GstMfxDisplayWaylandPrivate *const priv = data;

	if (strcmp(interface, "wl_compositor") == 0)
		priv->compositor =
		wl_registry_bind(registry, id, &wl_compositor_interface, 1);
	else if (strcmp(interface, "wl_shell") == 0)
		priv->shell = wl_registry_bind(registry, id, &wl_shell_interface, 1);
	else if (strcmp(interface, "wl_output") == 0) {
		priv->output = wl_registry_bind(registry, id, &wl_output_interface, 1);
		wl_output_add_listener(priv->output, &output_listener, priv);
	}
	else if (strcmp(interface, "wl_drm") == 0) {
		priv->drm = wl_registry_bind(registry, id, &wl_drm_interface, 2);
		wl_drm_add_listener(priv->drm, &drm_listener, priv);
	}
	else if(strcmp(interface, "wl_scaler") == 0) {
		priv->scaler = wl_registry_bind(registry, id, &wl_scaler_interface, 2);
	}
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	NULL,
};

static gboolean
gst_mfx_display_wayland_setup(GstMfxDisplay * display)
{
	GstMfxDisplayWaylandPrivate *const priv =
		GST_MFX_DISPLAY_WAYLAND_GET_PRIVATE(display);

	wl_display_set_user_data(priv->wl_display, priv);
	priv->registry = wl_display_get_registry(priv->wl_display);
	wl_registry_add_listener(priv->registry, &registry_listener, priv);
	priv->event_fd = wl_display_get_fd(priv->wl_display);
	wl_display_roundtrip(priv->wl_display);
	if (!priv->width || !priv->height) {
		wl_display_roundtrip(priv->wl_display);
		if (!priv->width || !priv->height) {
			GST_ERROR("failed to determine the display size");
			return FALSE;
		}
	}

	if (!priv->compositor) {
		GST_ERROR("failed to bind compositor interface");
		return FALSE;
	}
	
	if (!priv->is_auth) {
		wl_display_roundtrip(priv->wl_display);
	}

	if (!priv->shell) {
		GST_ERROR("failed to bind shell interface");
		return FALSE;
	}
	return TRUE;
}

static gboolean
gst_mfx_display_wayland_bind_display(GstMfxDisplay * display,
    gpointer native_display)
{
	GstMfxDisplayWaylandPrivate *const priv =
		GST_MFX_DISPLAY_WAYLAND_GET_PRIVATE(display);

	priv->wl_display = native_display;
	priv->use_foreign_display = TRUE;

	/* XXX: how to get socket/display name? */
	GST_WARNING("wayland: get display name");
	set_display_name(display, NULL);

	return gst_mfx_display_wayland_setup(display);
}

static gboolean
gst_mfx_display_wayland_open_display(GstMfxDisplay * display,
    const gchar * name)
{
	GstMfxDisplayWaylandPrivate *const priv =
		GST_MFX_DISPLAY_WAYLAND_GET_PRIVATE(display);

	if (!set_display_name(display, name))
		return FALSE;

    priv->wl_display = wl_display_connect(name);
    if (!priv->wl_display)
        return FALSE;
    priv->use_foreign_display = FALSE;

	return gst_mfx_display_wayland_setup(display);
}

static void
gst_mfx_display_wayland_close_display(GstMfxDisplay * display)
{
	GstMfxDisplayWaylandPrivate *const priv =
		GST_MFX_DISPLAY_WAYLAND_GET_PRIVATE(display);

	if (priv->bufmgr) {
        drm_intel_bufmgr_destroy(priv->bufmgr);
		priv->bufmgr = NULL;
	}
	
	if (priv->drm) {
		wl_drm_destroy(priv->drm);
		close(priv->drm_fd);
		g_free(priv->drm_device_name);
		priv->drm = NULL;
	}
	
	if (priv->output) {
		wl_output_destroy(priv->output);
		priv->output = NULL;
	}

	if (priv->shell) {
		wl_shell_destroy(priv->shell);
		priv->shell = NULL;
	}

	if (priv->compositor) {
		wl_compositor_destroy(priv->compositor);
		priv->compositor = NULL;
	}

	if (priv->registry) {
		wl_registry_destroy(priv->registry);
		priv->registry = NULL;
	}

	if (priv->wl_display) {
		if (!priv->use_foreign_display)
			wl_display_disconnect(priv->wl_display);
		priv->wl_display = NULL;
	}

	if (priv->display_name) {
		g_free(priv->display_name);
		priv->display_name = NULL;
	}
}

static gboolean
gst_mfx_display_wayland_get_display_info(GstMfxDisplay * display,
    GstMfxDisplayInfo * info)
{
	GstMfxDisplayWaylandPrivate *const priv =
		GST_MFX_DISPLAY_WAYLAND_GET_PRIVATE(display);

	/* Otherwise, create VA display if there is none already */
	info->native_display = priv->wl_display;
	info->display_name = priv->display_name;
	info->display_type = GST_MFX_DISPLAY_TYPE_WAYLAND;
	return TRUE;
}

static void
gst_mfx_display_wayland_get_size(GstMfxDisplay * display,
    guint * pwidth, guint * pheight)
{
	GstMfxDisplayWaylandPrivate *const priv =
		GST_MFX_DISPLAY_WAYLAND_GET_PRIVATE(display);

	if (!priv->output)
		return;

	if (pwidth)
		*pwidth = priv->width;

	if (pheight)
		*pheight = priv->height;
}

static void
gst_mfx_display_wayland_get_size_mm(GstMfxDisplay * display,
    guint * pwidth, guint * pheight)
{
	GstMfxDisplayWaylandPrivate *const priv =
		GST_MFX_DISPLAY_WAYLAND_GET_PRIVATE(display);

	if (!priv->output)
		return;

	if (pwidth)
		*pwidth = priv->phys_width;

	if (pheight)
		*pheight = priv->phys_height;
}

static GstMfxWindow *
gst_mfx_display_wayland_create_window(GstMfxDisplay * display,
    guint width, guint height)
{
	return gst_mfx_window_wayland_new(display, width, height);
}

static void
gst_mfx_display_wayland_init(GstMfxDisplay * display)
{
	GstMfxDisplayWaylandPrivate *const priv =
		GST_MFX_DISPLAY_WAYLAND_GET_PRIVATE(display);

	priv->event_fd = -1;
	priv->is_auth = FALSE;
}

static void
gst_mfx_display_wayland_class_init(GstMfxDisplayWaylandClass * klass)
{
	GstMfxMiniObjectClass *const object_class =
		GST_MFX_MINI_OBJECT_CLASS(klass);
	GstMfxDisplayClass *const dpy_class = GST_MFX_DISPLAY_CLASS(klass);

	gst_mfx_display_class_init(&klass->parent_class);

	object_class->size = sizeof (GstMfxDisplayWayland);
	dpy_class->display_type = GST_MFX_DISPLAY_TYPE_WAYLAND;
	dpy_class->init = gst_mfx_display_wayland_init;
	dpy_class->bind_display = gst_mfx_display_wayland_bind_display;
	dpy_class->open_display = gst_mfx_display_wayland_open_display;
	dpy_class->close_display = gst_mfx_display_wayland_close_display;
	dpy_class->get_display = gst_mfx_display_wayland_get_display_info;
	dpy_class->get_size = gst_mfx_display_wayland_get_size;
	dpy_class->get_size_mm = gst_mfx_display_wayland_get_size_mm;
	dpy_class->create_window = gst_mfx_display_wayland_create_window;
}

static inline const GstMfxDisplayClass *
gst_mfx_display_wayland_class(void)
{
	static GstMfxDisplayWaylandClass g_class;
	static gsize g_class_init = FALSE;

	if (g_once_init_enter(&g_class_init)) {
		gst_mfx_display_wayland_class_init(&g_class);
		g_once_init_leave(&g_class_init, TRUE);
	}
	return GST_MFX_DISPLAY_CLASS(&g_class);
}

/**
* gst_mfx_display_wayland_new:
* @display_name: the Wayland display name
*
* Opens an Wayland #wl_display using @display_name and returns a
* newly allocated #GstMfxDisplay object. The Wayland display will
* be cloed when the reference count of the object reaches zero.
*
* Return value: a newly allocated #GstMfxDisplay object
*/
GstMfxDisplay *
gst_mfx_display_wayland_new(const gchar * display_name)
{
	return gst_mfx_display_new(gst_mfx_display_wayland_class(),
		GST_MFX_DISPLAY_INIT_FROM_DISPLAY_NAME, (gpointer)display_name);
}

/**
* gst_mfx_display_wayland_new_with_display:
* @wl_display: an Wayland #wl_display
*
* Creates a #GstMfxDisplay based on the Wayland @wl_display
* display. The caller still owns the display and must call
* wl_display_disconnect() when all #GstMfxDisplay references are
* released. Doing so too early can yield undefined behaviour.
*
* Return value: a newly allocated #GstMfxDisplay object
*/
GstMfxDisplay *
gst_mfx_display_wayland_new_with_display(struct wl_display * wl_display)
{
	g_return_val_if_fail(wl_display, NULL);

	return gst_mfx_display_new(gst_mfx_display_wayland_class(),
		GST_MFX_DISPLAY_INIT_FROM_NATIVE_DISPLAY, wl_display);
}

/**
* gst_mfx_display_wayland_get_display:
* @display: a #GstMfxDisplayWayland
*
* Returns the underlying Wayland #wl_display that was created by
* gst_mfx_display_wayland_new() or that was bound from
* gst_mfx_display_wayland_new_with_display().
*
* Return value: the Wayland #wl_display attached to @display
*/
struct wl_display *
	gst_mfx_display_wayland_get_display(GstMfxDisplayWayland * display)
{
		g_return_val_if_fail(GST_MFX_IS_DISPLAY_WAYLAND(display), NULL);

		return GST_MFX_DISPLAY_WL_DISPLAY(display);
}
