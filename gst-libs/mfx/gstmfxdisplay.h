#ifndef GST_MFX_DISPLAY_H
#define GST_MFX_DISPLAY_H

#include <va/va.h>
#include <gst/gst.h>
#include "gstmfxtypes.h"

G_BEGIN_DECLS

#define GST_MFX_DISPLAY(obj) \
	((GstMfxDisplay *)(obj))

/**
* GST_MFX_DISPLAY_GET_CLASS_TYPE:
* @display: a #GstMfxDisplay
*
* Returns the #display class type
*/
#define GST_MFX_DISPLAY_GET_CLASS_TYPE(display) \
	gst_mfx_display_get_class_type (GST_MFX_DISPLAY (display))

/**
* GST_MFX_DISPLAY_VADISPLAY_TYPE:
* @display: a #GstMfxDisplay
*
* Returns the underlying VADisplay @display type.
*/
#define GST_MFX_DISPLAY_VADISPLAY_TYPE(display) \
	gst_mfx_display_get_display_type (GST_MFX_DISPLAY (display))

/**
* GST_MFX_DISPLAY_VADISPLAY:
* @display_: a #GstMfxDisplay
*
* Macro that evaluates to the #VADisplay of @display.
*/
#define GST_MFX_DISPLAY_VADISPLAY(display) \
	gst_mfx_display_get_display (GST_MFX_DISPLAY (display))

/**
* GST_MFX_DISPLAY_LOCK:
* @display: a #GstMfxDisplay
*
* Locks @display
*/
#define GST_MFX_DISPLAY_LOCK(display) \
	gst_mfx_display_lock (GST_MFX_DISPLAY (display))

/**
* GST_MFX_DISPLAY_UNLOCK:
* @display: a #GstMfxDisplay
*
* Unlocks @display
*/
#define GST_MFX_DISPLAY_UNLOCK(display) \
	gst_mfx_display_unlock (GST_MFX_DISPLAY (display))

typedef struct _GstMfxDisplayInfo             GstMfxDisplayInfo;
typedef struct _GstMfxDisplay                 GstMfxDisplay;

typedef enum
{
	GST_MFX_DISPLAY_TYPE_ANY = 0,
	GST_MFX_DISPLAY_TYPE_X11,
	GST_MFX_DISPLAY_TYPE_GLX,
	GST_MFX_DISPLAY_TYPE_WAYLAND,
	GST_MFX_DISPLAY_TYPE_DRM,
	GST_MFX_DISPLAY_TYPE_EGL,
} GstMfxDisplayType;

#define GST_MFX_TYPE_DISPLAY_TYPE \
	(gst_mfx_display_type_get_type())

GType
gst_mfx_display_type_get_type(void);

gboolean
gst_mfx_display_type_is_compatible(GstMfxDisplayType type1,
	GstMfxDisplayType type2);

/**
* GstMfxDisplayInfo:
*
* Generic class to retrieve VA display info
*/
struct _GstMfxDisplayInfo
{
	GstMfxDisplay *display;
	GstMfxDisplayType display_type;
	gchar *display_name;
	VADisplay va_display;
	gpointer native_display;
};

GstMfxDisplay *
gst_mfx_display_new_with_display(VADisplay va_display);

GstMfxDisplay *
gst_mfx_display_ref(GstMfxDisplay * display);

void
gst_mfx_display_unref(GstMfxDisplay * display);

void
gst_mfx_display_replace(GstMfxDisplay ** old_display_ptr,
    GstMfxDisplay * new_display);

void
gst_mfx_display_lock(GstMfxDisplay * display);

void
gst_mfx_display_unlock(GstMfxDisplay * display);

void
gst_mfx_display_sync(GstMfxDisplay * display);

void
gst_mfx_display_flush(GstMfxDisplay * display);

GstMfxDisplayType
gst_mfx_display_get_class_type(GstMfxDisplay * display);

GstMfxDisplayType
gst_mfx_display_get_display_type(GstMfxDisplay * display);

const gchar *
gst_mfx_display_get_display_name(GstMfxDisplay * display);

VADisplay
gst_mfx_display_get_display(GstMfxDisplay * display);

guint
gst_mfx_display_get_width(GstMfxDisplay * display);

guint
gst_mfx_display_get_height(GstMfxDisplay * display);

void
gst_mfx_display_get_size(GstMfxDisplay * display, guint * pwidth,
	guint * pheight);

void
gst_mfx_display_get_pixel_aspect_ratio(GstMfxDisplay * display,
	guint * par_n, guint * par_d);

const gchar *
gst_mfx_display_get_vendor_string(GstMfxDisplay * display);

gboolean
gst_mfx_display_has_opengl(GstMfxDisplay * display);

G_END_DECLS

#endif /* GST_MFX_DISPLAY_H */
