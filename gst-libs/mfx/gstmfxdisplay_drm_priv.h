#ifndef GST_MFX_DISPLAY_DRM_PRIV_H
#define GST_MFX_DISPLAY_DRM_PRIV_H

#include "gstmfxdisplay_drm.h"
#include "gstmfxdisplay_priv.h"

G_BEGIN_DECLS

#define GST_MFX_IS_DISPLAY_DRM(display) \
	((display) != NULL && \
	GST_MFX_DISPLAY_VADISPLAY_TYPE (display) == GST_MFX_DISPLAY_TYPE_DRM)

#define GST_MFX_DISPLAY_DRM_CAST(display) \
	((GstMfxDisplayDRM *)(display))

#define GST_MFX_DISPLAY_DRM_PRIVATE(display) \
	(&GST_MFX_DISPLAY_DRM_CAST(display)->priv)

typedef struct _GstMfxDisplayDRMPrivate       GstMfxDisplayDRMPrivate;
typedef struct _GstMfxDisplayDRMClass         GstMfxDisplayDRMClass;

/**
 * GST_MFX_DISPLAY_DRM_DEVICE
 * @display: a #GstMfxDisplay
 *
 * Macro that evaluated to the underlying DRM file descriptor of @display
 */
#undef  GST_MFX_DISPLAY_DRM_DEVICE
#define GST_MFX_DISPLAY_DRM_DEVICE(display) \
    GST_MFX_DISPLAY_DRM_PRIVATE(display)->drm_device

struct _GstMfxDisplayDRMPrivate
{
    gchar *device_path_default;
    gchar *device_path;
    gint drm_device;
    guint use_foreign_display:1;
};

/**
* GstMfxDisplayDRM:
*
* VA/DRM display wrapper.
*/
struct _GstMfxDisplayDRM
{
	/*< private >*/
	GstMfxDisplay parent_instance;

	GstMfxDisplayDRMPrivate priv;
};

/**
* GstMfxDisplayDRMClass:
*
* VA/X11 display wrapper clas.
*/
struct _GstMfxDisplayDRMClass
{
	/*< private >*/
	GstMfxDisplayClass parent_class;
};


G_END_DECLS

#endif /* GST_MFX_DISPLAY_DRM_PRIV_H */
