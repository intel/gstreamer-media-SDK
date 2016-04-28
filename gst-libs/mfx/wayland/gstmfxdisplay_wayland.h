#ifndef GST_MFX_DISPLAY_WAYLAND_H
#define GST_MFX_DISPLAY_WAYLAND_H

#include <wayland-client.h>
#include "gstmfxdisplay.h"

G_BEGIN_DECLS

#define GST_MFX_DISPLAY_WAYLAND(obj) \
	((GstMfxDisplayWayland *)(obj))

typedef struct _GstMfxDisplayWayland          GstMfxDisplayWayland;

GstMfxDisplay *
gst_mfx_display_wayland_new(const gchar * display_name);

GstMfxDisplay *
gst_mfx_display_wayland_new_with_display(struct wl_display * wl_display);

struct wl_display *
gst_mfx_display_wayland_get_display(GstMfxDisplayWayland * display);

G_END_DECLS

#endif /* GST_MFX_DISPLAY_WAYLAND_H */
