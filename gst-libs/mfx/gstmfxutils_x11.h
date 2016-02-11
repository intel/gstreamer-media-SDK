#ifndef GST_MFX_UTILS_X11_H
#define GST_MFX_UTILS_X11_H

#include <X11/Xlib.h>
#include <glib.h>
#include <gst/gst.h>

void
x11_trap_errors(void);

int
x11_untrap_errors(void);

Window
x11_create_window(Display * dpy, guint w, guint h, guint vid, Colormap cmap);

gboolean
x11_get_geometry(Display * dpy, Drawable drawable, gint * px, gint * py,
	guint * pwidth, guint * pheight, guint * pdepth);

#endif /* GST_MFX_UTILS_X11_H */
