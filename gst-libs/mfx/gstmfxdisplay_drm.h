#ifndef GST_MFX_DISPLAY_DRM_H
#define GST_MFX_DISPLAY_DRM_H

#include "gstmfxdisplay.h"

G_BEGIN_DECLS

#define GST_MFX_DISPLAY_DRM(obj) \
	((GstMfxDisplayDRM *)(obj))

typedef struct _GstMfxDisplayDRM              GstMfxDisplayDRM;

GstMfxDisplay *
gst_mfx_display_drm_new(const gchar * display_path);

GstMfxDisplay *
gst_mfx_display_drm_new_with_device(gint device);

gint 
gst_mfx_display_drm_get_device(GstMfxDisplayDRM * display);

const gchar *
gst_mfx_display_drm_get_device_path(GstMfxDisplayDRM * display);

G_END_DECLS


#endif /* GST_MFX_DISPLAY_DRM_H */
