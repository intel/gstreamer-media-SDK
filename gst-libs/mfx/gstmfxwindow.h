#ifndef GST_MFX_WINDOW_H
#define GST_MFX_WINDOW_H

#include <gst/video/gstvideosink.h>
#include "gstmfxtypes.h"
#include "gstmfxdisplay.h"
#include "gstmfxsurface.h"

G_BEGIN_DECLS

#define GST_MFX_WINDOW(obj) \
	((GstMfxWindow *)(obj))

typedef struct _GstMfxWindow GstMfxWindow;
typedef struct _GstMfxWindowClass GstMfxWindowClass;

GstMfxWindow *
gst_mfx_window_new(GstMfxDisplay * display, guint width, guint height);

GstMfxWindow *
gst_mfx_window_ref(GstMfxWindow * window);

void
gst_mfx_window_unref(GstMfxWindow * window);

void
gst_mfx_window_replace(GstMfxWindow ** old_window_ptr,
	GstMfxWindow * new_window);

GstMfxDisplay *
gst_mfx_window_get_display(GstMfxWindow * window);

void
gst_mfx_window_show(GstMfxWindow * window);

void
gst_mfx_window_hide(GstMfxWindow * window);

gboolean
gst_mfx_window_get_fullscreen(GstMfxWindow * window);

void
gst_mfx_window_set_fullscreen(GstMfxWindow * window, gboolean fullscreen);

guint
gst_mfx_window_get_width(GstMfxWindow * window);

guint
gst_mfx_window_get_height(GstMfxWindow * window);

void
gst_mfx_window_get_size(GstMfxWindow * window, guint * width_ptr,
	guint * height_ptr);

void
gst_mfx_window_set_width(GstMfxWindow * window, guint width);

void
gst_mfx_window_set_height(GstMfxWindow * window, guint height);

void
gst_mfx_window_set_size(GstMfxWindow * window, guint width, guint height);

gboolean
gst_mfx_window_put_surface(GstMfxWindow * window,
	GstMfxSurface * surface, const GstMfxRectangle * src_rect,
	const GstMfxRectangle * dst_rect);

void
gst_mfx_window_reconfigure(GstMfxWindow * window);

gboolean
gst_mfx_window_unblock(GstMfxWindow * window);

gboolean
gst_mfx_window_unblock_cancel(GstMfxWindow * window);

G_END_DECLS

#endif /* GST_MFX_WINDOW_H */
