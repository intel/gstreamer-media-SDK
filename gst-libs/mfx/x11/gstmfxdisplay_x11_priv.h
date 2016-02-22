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

struct _GstMfxDisplayX11Private
{
	gchar *display_name;
	Display *x11_display;
	int x11_screen;
	GArray *pixel_formats;
	guint use_foreign_display : 1;  // Foreign native_display?
	guint use_xrandr : 1;
	guint synchronous : 1;
	gchar *device_path_default;
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


G_END_DECLS

#endif /* GST_MFX_DISPLAY_X11_PRIV_H */
