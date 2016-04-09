#ifndef GST_MFX_WINDOW_WAYLAND_H
#define GST_MFX_WINDOW_WAYLAND_H

#include "gstmfxdisplay.h"
#include "gstmfxwindow.h"

G_BEGIN_DECLS

typedef struct _GstMfxWindowWayland GstMfxWindowWayland;

GstMfxWindow *
gst_mfx_window_wayland_new(GstMfxDisplay * display, guint width,
	guint height);

G_END_DECLS

#endif /* GST_MFX_WINDOW_WAYLAND_H */
