#ifndef GST_MFX_WINDOW_X11_H
#define GST_MFX_WINDOW_X11_H

#include <X11/Xlib.h>
#include "gstmfxdisplay.h"
#include "gstmfxwindow.h"

G_BEGIN_DECLS

#define GST_MFX_WINDOW_X11(obj) \
	((GstMfxWindowX11 *)(obj))

/**
* GST_MFX_WINDOW_XWINDOW:
* @window: a #GstMfxWindow
*
* Macro that evaluates to the underlying X11 #Window of @window
*/
#define GST_MFX_WINDOW_XWINDOW(window) \
	gst_mfx_window_x11_get_xid(GST_MFX_WINDOW_X11(window))

typedef struct _GstMfxWindowX11 GstMfxWindowX11;

GstMfxWindow *
gst_mfx_window_x11_new(GstMfxDisplay * display, guint width, guint height);

Window
gst_mfx_window_x11_get_xid(GstMfxWindowX11 * window);

G_END_DECLS

#endif /* GST_MFX_WINDOW_X11_H */