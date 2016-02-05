#ifndef GST_MFX_DISPLAY_WAYLAND_PRIV_H
#define GST_MFX_DISPLAY_WAYLAND_PRIV_H

#include <intel_bufmgr.h>
#include "gstmfxdisplay_wayland.h"
#include "gstmfxdisplay_priv.h"
#include "wayland-drm-client-protocol.h"

G_BEGIN_DECLS

#define GST_MFX_IS_DISPLAY_WAYLAND(display) \
	((display) != NULL && \
	GST_MFX_DISPLAY_VADISPLAY_TYPE (display) == GST_MFX_DISPLAY_TYPE_WAYLAND)

#define GST_MFX_DISPLAY_WAYLAND_CAST(display) \
	((GstMfxDisplayWayland *)(display))

#define GST_MFX_DISPLAY_WAYLAND_GET_PRIVATE(display) \
	(&GST_MFX_DISPLAY_WAYLAND_CAST(display)->priv)

#ifndef BATCH_SIZE
#define BATCH_SIZE 0x80000
#endif 

typedef struct _GstMfxDisplayWaylandPrivate   GstMfxDisplayWaylandPrivate;
typedef struct _GstMfxDisplayWaylandClass     GstMfxDisplayWaylandClass;

/**
* GST_MFX_DISPLAY_WL_DISPLAY:
* @display: a #GstMfxDisplay
*
* Macro that evaluates to the underlying Wayland #wl_display object
* of @display
*/
#undef  GST_MFX_DISPLAY_WL_DISPLAY
#define GST_MFX_DISPLAY_WL_DISPLAY(display) \
	GST_MFX_DISPLAY_WAYLAND_GET_PRIVATE(display)->wl_display

struct _GstMfxDisplayWaylandPrivate
{
	gchar *display_name;
	struct wl_display *wl_display;
	struct wl_compositor *compositor;
	struct wl_shell *shell;
	struct wl_output *output;
	struct wl_registry *registry;
	struct wl_drm *drm;
	guint width;
	guint height;
	guint phys_width;
	guint phys_height;
	gint event_fd;
	gint drm_fd;
	gchar *drm_device_name;	
	drm_intel_bufmgr *bufmgr;
	gboolean is_auth;
	guint use_foreign_display : 1;
};

/**
* GstMfxDisplayWayland:
*
* VA/Wayland display wrapper.
*/
struct _GstMfxDisplayWayland
{
	/*< private >*/
	GstMfxDisplay parent_instance;

	GstMfxDisplayWaylandPrivate priv;
};

/**
* GstMfxDisplayWaylandClass:
*
* VA/Wayland display wrapper clas.
*/
struct _GstMfxDisplayWaylandClass
{
	/*< private >*/
	GstMfxDisplayClass parent_class;
};

G_END_DECLS

#endif /* GST_MFX_DISPLAY_WAYLAND_PRIV_H */
