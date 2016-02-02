#include "sysdeps.h"
#include <string.h>
#include <fcntl.h>
#include <libudev.h>
#include <xf86drm.h>
#include <va/va_drm.h>
#include "gstmfxdisplay.h"
#include "gstmfxdisplay_priv.h"

/* Ensure those symbols are actually defined in the resulting libraries */
#undef gst_mfx_display_ref
#undef gst_mfx_display_unref
#undef gst_mfx_display_replace

static VADisplay g_va_display;

/* Get default device path. Actually, the first match in the DRM subsystem */
const gchar *
get_default_device_path (GstMfxDisplay * display)
{
	GstMfxDisplayPrivate *const priv =
		GST_MFX_DISPLAY_GET_PRIVATE(display);
    const gchar *sysnames[] = {"renderD[0-9]*", "card[0-9]*", NULL};
	const gchar *syspath, *devpath;
	struct udev *udev = NULL;
	struct udev_device *device, *parent;
	struct udev_enumerate *e = NULL;
	struct udev_list_entry *l;
	int fd;
	guint i;

	if (!priv->device_path_default) {
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
                fd = open (devpath, O_RDWR | O_CLOEXEC);
                if (fd < 0) {
                    udev_device_unref (device);
                    continue;
                }

                priv->device_path_default = g_strdup (devpath);
                close (fd);
                udev_device_unref (device);
                break;
            }
            if (priv->device_path_default)
                break;
        }
    }

end:
    if (e)
        udev_enumerate_unref (e);
    if (udev)
        udev_unref (udev);
	return priv->device_path_default;
}

/* GstMfxDisplayType enumerations */
GType
gst_mfx_display_type_get_type(void)
{
	static GType g_type = 0;

	static const GEnumValue display_types[] = {
		{ GST_MFX_DISPLAY_TYPE_ANY,
		"Auto detection", "any" },
#if USE_EGL
		{ GST_MFX_DISPLAY_TYPE_EGL,
		"VA/EGL display", "egl" },
#endif
#if USE_WAYLAND
        { GST_MFX_DISPLAY_TYPE_WAYLAND,
		"VA/Wayland display", "wayland" },
#endif
#if USE_DRM
		{ GST_MFX_DISPLAY_TYPE_DRM,
		"VA/DRM display", "drm" },
#endif
		{ 0, NULL, NULL },
	};

	if (!g_type)
		g_type = g_enum_register_static("GstMfxDisplayType", display_types);
	return g_type;
}

/**
* gst_mfx_display_type_is_compatible:
* @type1: the #GstMfxDisplayType to test
* @type2: the reference #GstMfxDisplayType
*
* Compares whether #GstMfxDisplay @type1 is compatible with @type2.
* That is, if @type2 is in "any" category, or derived from @type1.
*
* Returns: %TRUE if @type1 is compatible with @type2, %FALSE otherwise.
*/
gboolean
gst_mfx_display_type_is_compatible(GstMfxDisplayType type1,
	GstMfxDisplayType type2)
{
	if (type1 == type2)
		return TRUE;

	switch (type1) {
	case GST_MFX_DISPLAY_TYPE_X11:
	case GST_MFX_DISPLAY_TYPE_WAYLAND:
		if (type2 == GST_MFX_DISPLAY_TYPE_EGL)
			return TRUE;
		break;
	default:
		break;
	}
	return type2 == GST_MFX_DISPLAY_TYPE_ANY;
}

static void
gst_mfx_display_calculate_pixel_aspect_ratio(GstMfxDisplay * display)
{
	GstMfxDisplayPrivate *const priv = GST_MFX_DISPLAY_GET_PRIVATE(display);
	gdouble ratio, delta;
	gint i, j, index, windex;

	static const gint par[][2] = {
		{ 1, 1 },                     /* regular screen            */
		{ 16, 15 },                   /* PAL TV                    */
		{ 11, 10 },                   /* 525 line Rec.601 video    */
		{ 54, 59 },                   /* 625 line Rec.601 video    */
		{ 64, 45 },                   /* 1280x1024 on 16:9 display */
		{ 5, 3 },                     /* 1280x1024 on  4:3 display */
		{ 4, 3 }                      /*  800x600  on 16:9 display */
	};

	/* First, calculate the "real" ratio based on the X values;
	* which is the "physical" w/h divided by the w/h in pixels of the
	* display */
	if (!priv->width || !priv->height || !priv->width_mm || !priv->height_mm)
		ratio = 1.0;
	else
		ratio = (gdouble)(priv->width_mm * priv->height) /
		(priv->height_mm * priv->width);
	GST_DEBUG("calculated pixel aspect ratio: %f", ratio);

	/* Now, find the one from par[][2] with the lowest delta to the real one */
#define DELTA(idx, w) (ABS(ratio - ((gdouble)par[idx][w] / par[idx][!(w)])))
	delta = DELTA(0, 0);
	index = 0;
	windex = 0;

	for (i = 1; i < G_N_ELEMENTS(par); i++) {
		for (j = 0; j < 2; j++) {
			const gdouble this_delta = DELTA(i, j);
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
gst_mfx_display_destroy(GstMfxDisplay * display)
{
	GstMfxDisplayPrivate *const priv = GST_MFX_DISPLAY_GET_PRIVATE(display);

	if (priv->display) {
		if (!priv->parent)
			vaTerminate(priv->display);
		priv->display = NULL;
	}

	if (!priv->use_foreign_display) {
		GstMfxDisplayClass *klass = GST_MFX_DISPLAY_GET_CLASS(display);
		if (klass->close_display)
			klass->close_display(display);
	}

	g_free(priv->display_name);
	priv->display_name = NULL;

	g_free(priv->vendor_string);
	priv->vendor_string = NULL;

	g_free(priv->device_path_default);
	priv->device_path_default = NULL;

	gst_mfx_display_replace_internal(&priv->parent, NULL);
}

static gboolean
gst_mfx_display_create(GstMfxDisplay * display,
	GstMfxDisplayInitType init_type, gpointer init_value)
{
	GstMfxDisplayPrivate *const priv = GST_MFX_DISPLAY_GET_PRIVATE(display);
	const GstMfxDisplayClass *const klass =
		GST_MFX_DISPLAY_GET_CLASS(display);
	gint major_version, minor_version;
	VAStatus status;
	GstMfxDisplayInfo info;

	memset(&info, 0, sizeof (info));
	info.display = display;
	info.display_type = priv->display_type;

	switch (init_type) {
	case GST_MFX_DISPLAY_INIT_FROM_DISPLAY_NAME:
		if (klass->open_display && !klass->open_display(display, init_value))
			return FALSE;
		goto create_display;
	case GST_MFX_DISPLAY_INIT_FROM_NATIVE_DISPLAY:
		if (klass->bind_display && !klass->bind_display(display, init_value))
			return FALSE;
		// fall-through
	create_display:
		if (!klass->get_display || !klass->get_display(display, &info))
			return FALSE;
		priv->display_type = info.display_type;
		priv->native_display = info.native_display;
		if (klass->get_size)
			klass->get_size(display, &priv->width, &priv->height);
		if (klass->get_size_mm)
			klass->get_size_mm(display, &priv->width_mm, &priv->height_mm);
		gst_mfx_display_calculate_pixel_aspect_ratio(display);
		break;
	}
	//if (!priv->display)
		//return FALSE;

	//if (!priv->parent) {
    if (!g_va_display) {
        int fd = open(get_default_device_path(display), O_RDWR | O_CLOEXEC);
        g_va_display = vaGetDisplayDRM (fd);
        if (!g_va_display)
            return FALSE;

		status = vaInitialize(g_va_display, &major_version, &minor_version);
		if (!vaapi_check_status(status, "vaInitialize()"))
			return FALSE;
		GST_DEBUG("VA-API version %d.%d", major_version, minor_version);
	}
	priv->display = g_va_display;

	g_free(priv->display_name);
	priv->display_name = g_strdup(info.display_name);
	return TRUE;
}

static void
gst_mfx_display_lock_default(GstMfxDisplay * display)
{
	GstMfxDisplayPrivate *priv = GST_MFX_DISPLAY_GET_PRIVATE(display);

	if (priv->parent)
		priv = GST_MFX_DISPLAY_GET_PRIVATE(priv->parent);
    g_rec_mutex_lock(&priv->mutex);
    /*if(g_rec_mutex_trylock(&priv->mutex))
        printf("locked display %s\n", priv->display_name);
    else
        printf("failed to display %s\n", priv->display_name);*/
}

static void
gst_mfx_display_unlock_default(GstMfxDisplay * display)
{
	GstMfxDisplayPrivate *priv = GST_MFX_DISPLAY_GET_PRIVATE(display);

	if (priv->parent)
		priv = GST_MFX_DISPLAY_GET_PRIVATE(priv->parent);
	g_rec_mutex_unlock(&priv->mutex);
	//printf("unlocked display %s\n", priv->display_name);
}

static void
gst_mfx_display_init(GstMfxDisplay * display)
{
	GstMfxDisplayPrivate *const priv = GST_MFX_DISPLAY_GET_PRIVATE(display);
	const GstMfxDisplayClass *const dpy_class =
		GST_MFX_DISPLAY_GET_CLASS(display);

	priv->display_type = GST_MFX_DISPLAY_TYPE_ANY;
	priv->par_n = 1;
	priv->par_d = 1;

	g_rec_mutex_init(&priv->mutex);

	if (dpy_class->init)
		dpy_class->init(display);
}

static void
gst_mfx_display_finalize(GstMfxDisplay * display)
{
	GstMfxDisplayPrivate *const priv = GST_MFX_DISPLAY_GET_PRIVATE(display);

	gst_mfx_display_destroy(display);
	g_rec_mutex_clear(&priv->mutex);
}

void
gst_mfx_display_class_init(GstMfxDisplayClass * klass)
{
	GstMfxMiniObjectClass *const object_class =
		GST_MFX_MINI_OBJECT_CLASS(klass);
	GstMfxDisplayClass *const dpy_class = GST_MFX_DISPLAY_CLASS(klass);

	object_class->size = sizeof (GstMfxDisplay);
	object_class->finalize = (GDestroyNotify)gst_mfx_display_finalize;
	dpy_class->lock = gst_mfx_display_lock_default;
	dpy_class->unlock = gst_mfx_display_unlock_default;
}

static inline const GstMfxDisplayClass *
gst_mfx_display_class(void)
{
	static GstMfxDisplayClass g_class;
	static gsize g_class_init = FALSE;

	if (g_once_init_enter(&g_class_init)) {
		gst_mfx_display_class_init(&g_class);
		g_once_init_leave(&g_class_init, TRUE);
	}
	return &g_class;
}

GstMfxDisplay *
gst_mfx_display_new(const GstMfxDisplayClass * klass,
	GstMfxDisplayInitType init_type, gpointer init_value)
{
	GstMfxDisplay *display;

	display = (GstMfxDisplay *)
		gst_mfx_mini_object_new0(GST_MFX_MINI_OBJECT_CLASS(klass));
	if (!display)
		return NULL;

	gst_mfx_display_init(display);
	if (!gst_mfx_display_create(display, init_type, init_value))
		goto error;
	return display;

error:
	gst_mfx_display_unref_internal(display);
	return NULL;
}

/**
* gst_mfx_display_ref:
* @display: a #GstMfxDisplay
*
* Atomically increases the reference count of the given @display by one.
*
* Returns: The same @display argument
*/
GstMfxDisplay *
gst_mfx_display_ref(GstMfxDisplay * display)
{
	return gst_mfx_display_ref_internal(display);
}

/**
* gst_mfx_display_unref:
* @display: a #GstMfxDisplay
*
* Atomically decreases the reference count of the @display by one. If
* the reference count reaches zero, the display will be free'd.
*/
void
gst_mfx_display_unref(GstMfxDisplay * display)
{
	gst_mfx_display_unref_internal(display);
}

/**
* gst_mfx_display_replace:
* @old_display_ptr: a pointer to a #GstMfxDisplay
* @new_display: a #GstMfxDisplay
*
* Atomically replaces the display display held in @old_display_ptr
* with @new_display. This means that @old_display_ptr shall reference
* a valid display. However, @new_display can be NULL.
*/
void
gst_mfx_display_replace(GstMfxDisplay ** old_display_ptr,
	GstMfxDisplay * new_display)
{
	gst_mfx_display_replace_internal(old_display_ptr, new_display);
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
gst_mfx_display_lock(GstMfxDisplay * display)
{
	GstMfxDisplayClass *klass;

	g_return_if_fail(display != NULL);

	klass = GST_MFX_DISPLAY_GET_CLASS(display);
	if (klass->lock)
		klass->lock(display);
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
gst_mfx_display_unlock(GstMfxDisplay * display)
{
	GstMfxDisplayClass *klass;

	g_return_if_fail(display != NULL);

	klass = GST_MFX_DISPLAY_GET_CLASS(display);
	if (klass->unlock)
		klass->unlock(display);
}

/**
* gst_mfx_display_sync:
* @display: a #GstMfxDisplay
*
* Flushes any requests queued for the windowing system and waits until
* all requests have been handled. This is often used for making sure
* that the display is synchronized with the current state of the program.
*
* This is most useful for X11. On windowing systems where requests are
* handled synchronously, this function will do nothing.
*/
void
gst_mfx_display_sync(GstMfxDisplay * display)
{
	GstMfxDisplayClass *klass;

	g_return_if_fail(display != NULL);

	klass = GST_MFX_DISPLAY_GET_CLASS(display);
	if (klass->sync)
		klass->sync(display);
	else if (klass->flush)
		klass->flush(display);
}

/**
* gst_mfx_display_flush:
* @display: a #GstMfxDisplay
*
* Flushes any requests queued for the windowing system.
*
* This is most useful for X11. On windowing systems where requests
* are handled synchronously, this function will do nothing.
*/
void
gst_mfx_display_flush(GstMfxDisplay * display)
{
	GstMfxDisplayClass *klass;

	g_return_if_fail(display != NULL);

	klass = GST_MFX_DISPLAY_GET_CLASS(display);
	if (klass->flush)
		klass->flush(display);
}

/**
* gst_mfx_display_get_class_type:
* @display: a #GstMfxDisplay
*
* Returns the #GstMfxDisplayType of @display. This is the type of
* the object, thus the associated class, not the type of the VA
* display.
*
* Return value: the #GstMfxDisplayType
*/
GstMfxDisplayType
gst_mfx_display_get_class_type(GstMfxDisplay * display)
{
	g_return_val_if_fail(display != NULL, GST_MFX_DISPLAY_TYPE_ANY);

	return GST_MFX_DISPLAY_GET_CLASS_TYPE(display);
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
gst_mfx_display_get_display_type(GstMfxDisplay * display)
{
	g_return_val_if_fail(display != NULL, GST_MFX_DISPLAY_TYPE_ANY);

	return GST_MFX_DISPLAY_VADISPLAY_TYPE(display);
}

/**
* gst_mfx_display_get_display_type:
* @display: a #GstMfxDisplay
*
* Returns the @display name.
*
* Return value: the display name
*/
const gchar *
gst_mfx_display_get_display_name(GstMfxDisplay * display)
{
	g_return_val_if_fail(display != NULL, NULL);

	return GST_MFX_DISPLAY_GET_PRIVATE(display)->display_name;
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
gst_mfx_display_get_display(GstMfxDisplay * display)
{
	g_return_val_if_fail(display != NULL, NULL);

	return GST_MFX_DISPLAY_GET_PRIVATE(display)->display;
}

/**
* gst_mfx_display_get_width:
* @display: a #GstMfxDisplay
*
* Retrieves the width of a #GstMfxDisplay.
*
* Return value: the width of the @display, in pixels
*/
guint
gst_mfx_display_get_width(GstMfxDisplay * display)
{
	g_return_val_if_fail(display != NULL, 0);

	return GST_MFX_DISPLAY_GET_PRIVATE(display)->width;
}

/**
* gst_mfx_display_get_height:
* @display: a #GstMfxDisplay
*
* Retrieves the height of a #GstMfxDisplay
*
* Return value: the height of the @display, in pixels
*/
guint
gst_mfx_display_get_height(GstMfxDisplay * display)
{
	g_return_val_if_fail(display != NULL, 0);

	return GST_MFX_DISPLAY_GET_PRIVATE(display)->height;
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
gst_mfx_display_get_size(GstMfxDisplay * display, guint * pwidth,
guint * pheight)
{
	g_return_if_fail(GST_MFX_DISPLAY(display));

	if (pwidth)
		*pwidth = GST_MFX_DISPLAY_GET_PRIVATE(display)->width;

	if (pheight)
		*pheight = GST_MFX_DISPLAY_GET_PRIVATE(display)->height;
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
gst_mfx_display_get_pixel_aspect_ratio(GstMfxDisplay * display,
guint * par_n, guint * par_d)
{
	g_return_if_fail(display != NULL);

	if (par_n)
		*par_n = GST_MFX_DISPLAY_GET_PRIVATE(display)->par_n;

	if (par_d)
		*par_d = GST_MFX_DISPLAY_GET_PRIVATE(display)->par_d;
}

/* Ensures the VA driver vendor string was copied */
static gboolean
ensure_vendor_string(GstMfxDisplay * display)
{
	GstMfxDisplayPrivate *const priv = GST_MFX_DISPLAY_GET_PRIVATE(display);
	const gchar *vendor_string;

	GST_MFX_DISPLAY_LOCK(display);
	if (!priv->vendor_string) {
		vendor_string = vaQueryVendorString(priv->display);
		if (vendor_string)
			priv->vendor_string = g_strdup(vendor_string);
	}
	GST_MFX_DISPLAY_UNLOCK(display);
	return priv->vendor_string != NULL;
}

/**
* gst_mfx_display_get_vendor_string:
* @display: a #GstMfxDisplay
*
* Returns the VA driver vendor string attached to the supplied VA @display.
* The @display owns the vendor string, do *not* de-allocate it.
*
* This function is thread safe.
*
* Return value: the current #GstMfxRotation value
*/
const gchar *
gst_mfx_display_get_vendor_string(GstMfxDisplay * display)
{
	g_return_val_if_fail(display != NULL, NULL);

	if (!ensure_vendor_string(display))
		return NULL;
	return display->priv.vendor_string;
}

/**
* gst_mfx_display_has_opengl:
* @display: a #GstMfxDisplay
*
* Returns wether the @display that was created does support OpenGL
* context to be attached.
*
* This function is thread safe.
*
* Return value: %TRUE if the @display supports OpenGL context, %FALSE
*   otherwise
*/
gboolean
gst_mfx_display_has_opengl(GstMfxDisplay * display)
{
	GstMfxDisplayClass *klass;

	g_return_val_if_fail(display != NULL, FALSE);

	klass = GST_MFX_DISPLAY_GET_CLASS(display);
	return (klass->display_type == GST_MFX_DISPLAY_TYPE_EGL);
}
