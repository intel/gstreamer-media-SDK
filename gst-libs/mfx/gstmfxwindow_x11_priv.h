#ifndef GST_MFX_WINDOW_X11_PRIV_H
#define GST_MFX_WINDOW_X11_PRIV_H

#include "gstmfxwindow_priv.h"

G_BEGIN_DECLS

#define GST_MFX_WINDOW_X11_GET_PRIVATE(obj) \
	(&GST_MFX_WINDOW_X11(obj)->priv)

#define GST_MFX_WINDOW_X11_CLASS(klass) \
	((GstMfxWindowX11Class *)(klass))

#define GST_MFX_WINDOW_X11_GET_CLASS(obj) \
	GST_MFX_WINDOW_X11_CLASS(GST_MFX_WINDOW_GET_CLASS(obj))

typedef struct _GstMfxWindowX11Private GstMfxWindowX11Private;
typedef struct _GstMfxWindowX11Class GstMfxWindowX11Class;

/**
* GST_MFX_WINDOW_DISPLAY_X11:
* @object: a #GstMfxWindow
*
* Macro that evaluates to the #GstMfxDisplayX11 the @object is bound to.
* This is an internal macro that does not do any run-time type check
* and requires #include "gstmfxdisplay_x11_priv.h"
*/
#define GST_MFX_WINDOW_DISPLAY_X11(object) \
	GST_MFX_DISPLAY_X11_CAST (GST_MFX_WINDOW_DISPLAY (object))

struct _GstMfxWindowX11Private
{
	Atom atom_NET_WM_STATE;
	Atom atom_NET_WM_STATE_FULLSCREEN;

	guint is_mapped : 1;
	guint fullscreen_on_map : 1;
};

/**
* GstMfxWindowX11:
*
* An X11 #Window wrapper.
*/
struct _GstMfxWindowX11
{
	/*< private >*/
	GstMfxWindow parent_instance;

	GstMfxWindowX11Private priv;
};

/**
* GstMfxWindowX11Class:
*
* An X11 #Window wrapper class.
*/
struct _GstMfxWindowX11Class
{
	/*< private >*/
	GstMfxWindowClass parent_class;
};

void
gst_mfx_window_x11_class_init(GstMfxWindowX11Class * klass);


G_END_DECLS

#endif /* GST_MFX_WINDOW_X11_PRIV_H */
