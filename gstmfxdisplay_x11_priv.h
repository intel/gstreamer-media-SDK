#ifndef GST_MFX_DISPLAY_X11_PRIV_H
#define GST_MFX_DISPLAY_X11_PRIV_H

#include "gstmfxdisplay_x11.h"
#include "gstmfxdisplay_priv.h"

G_BEGIN_DECLS

#define GST_MFX_IS_DISPLAY_X11(display) \
	((display) != NULL && \
	GST_MFX_DISPLAY_VADISPLAY_TYPE (display) == GST_MFX_DISPLAY_TYPE_X11)

#define GST_MFX_DISPLAY_X11_CAST(display) \
	((GstMfxDisplayX11 *)(display))

#define GST_MFX_DISPLAY_X11_PRIVATE(display) \
	(&GST_MFX_DISPLAY_X11_CAST(display)->priv)

typedef struct _GstMfxDisplayX11Private       GstMfxDisplayX11Private;
typedef struct _GstMfxDisplayX11Class         GstMfxDisplayX11Class;

/**
* GST_MFX_DISPLAY_XDISPLAY:
* @display: a #GstMfxDisplay
*
* Macro that evaluates to the underlying X11 #Display of @display
*/
#undef  GST_MFX_DISPLAY_XDISPLAY
#define GST_MFX_DISPLAY_XDISPLAY(display) \
	GST_MFX_DISPLAY_X11_PRIVATE(display)->x11_display

/**
* GST_MFX_DISPLAY_XSCREEN:
* @display: a #GstMfxDisplay
*
* Macro that evaluates to the underlying X11 screen of @display
*/
#undef  GST_MFX_DISPLAY_XSCREEN
#define GST_MFX_DISPLAY_XSCREEN(display) \
	GST_MFX_DISPLAY_X11_PRIVATE(display)->x11_screen

/**
* GST_MFX_DISPLAY_HAS_XRENDER:
* @display: a #GstMfxDisplay
*
* Macro that evaluates to the existence of the XRender extension on
* @display server.
*/
#undef  GST_MFX_DISPLAY_HAS_XRENDER
#define GST_MFX_DISPLAY_HAS_XRENDER(display) \
	(GST_MFX_DISPLAY_X11_PRIVATE(display)->has_xrender)

struct _GstMfxDisplayX11Private
{
	gchar *display_name;
	Display *x11_display;
	int x11_screen;
	GArray *pixel_formats;
	guint use_foreign_display : 1;  // Foreign native_display?
	guint use_xrandr : 1;
	guint has_xrender : 1;          // Has XRender extension?
	guint synchronous : 1;
};

/**
* GstMfxDisplayX11:
*
* VA/X11 display wrapper.
*/
struct _GstMfxDisplayX11
{
	/*< private >*/
	GstMfxDisplay parent_instance;

	GstMfxDisplayX11Private priv;
};

/**
* GstMfxDisplayX11Class:
*
* VA/X11 display wrapper clas.
*/
struct _GstMfxDisplayX11Class
{
	/*< private >*/
	GstMfxDisplayClass parent_class;
};

void
gst_mfx_display_x11_class_init(GstMfxDisplayX11Class * klass);

/*GstVideoFormat
gst_mfx_display_x11_get_pixmap_format(GstMfxDisplayX11 * display,
	guint depth);

guint
gst_mfx_display_x11_get_pixmap_depth(GstMfxDisplayX11 * display,
	GstVideoFormat format);*/

G_END_DECLS

#endif /* GST_MFX_DISPLAY_X11_PRIV_H */
