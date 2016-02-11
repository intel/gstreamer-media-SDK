#ifndef GST_MFX_DISPLAY_X11_H
#define GST_MFX_DISPLAY_X11_H

#include <va/va_x11.h>
#include "gstmfxdisplay.h"

G_BEGIN_DECLS

#define GST_MFX_DISPLAY_X11(obj) \
	((GstMfxDisplayX11 *)(obj))

typedef struct _GstMfxDisplayX11              GstMfxDisplayX11;

GstMfxDisplay *
gst_mfx_display_x11_new(const gchar * display_name);

GstMfxDisplay *
gst_mfx_display_x11_new_with_display(Display * x11_display);

Display *
gst_mfx_display_x11_get_display(GstMfxDisplayX11 * display);

int
gst_mfx_display_x11_get_screen(GstMfxDisplayX11 * display);

void
gst_mfx_display_x11_set_synchronous(GstMfxDisplayX11 * display,
	gboolean synchronous);

G_END_DECLS


#endif /* GST_VAAPI_DISPLAY_X11_H */