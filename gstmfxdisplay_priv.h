#ifndef GST_MFX_DISPLAY_PRIV_H
#define GST_MFX_DISPLAY_PRIV_H

#include "gstmfxdisplay.h"
#include "gstmfxdisplaycache.h"
#include "gstmfxwindow.h"
//#include "gstmfxtexture.h"
#include "gstmfxminiobject.h"

G_BEGIN_DECLS

#define GST_MFX_DISPLAY_CAST(display) \
	((GstMfxDisplay *) (display))

#define GST_MFX_DISPLAY_GET_PRIVATE(display) \
	(&GST_MFX_DISPLAY_CAST (display)->priv)

#define GST_MFX_DISPLAY_CLASS(klass) \
	((GstMfxDisplayClass *) (klass))

#define GST_MFX_IS_DISPLAY_CLASS(klass) \
	((klass) != NULL)

#define GST_MFX_DISPLAY_GET_CLASS(obj) \
	GST_MFX_DISPLAY_CLASS (GST_MFX_MINI_OBJECT_GET_CLASS (obj))

typedef struct _GstMfxDisplayPrivate          GstMfxDisplayPrivate;
typedef struct _GstMfxDisplayClass            GstMfxDisplayClass;
typedef enum _GstMfxDisplayInitType           GstMfxDisplayInitType;

typedef void(*GstMfxDisplayInitFunc) (GstMfxDisplay * display);
typedef gboolean(*GstMfxDisplayBindFunc) (GstMfxDisplay * display,
	gpointer native_dpy);
typedef gboolean(*GstMfxDisplayOpenFunc) (GstMfxDisplay * display,
	const gchar * name);
typedef void(*GstMfxDisplayCloseFunc) (GstMfxDisplay * display);
typedef void(*GstMfxDisplayLockFunc) (GstMfxDisplay * display);
typedef void(*GstMfxDisplayUnlockFunc) (GstMfxDisplay * display);
typedef void(*GstMfxDisplaySyncFunc) (GstMfxDisplay * display);
typedef void(*GstMfxDisplayFlushFunc) (GstMfxDisplay * display);
typedef gboolean(*GstMfxDisplayGetInfoFunc) (GstMfxDisplay * display,
	GstMfxDisplayInfo * info);
typedef void(*GstMfxDisplayGetSizeFunc) (GstMfxDisplay * display,
	guint * pwidth, guint * pheight);
typedef void(*GstMfxDisplayGetSizeMFunc) (GstMfxDisplay * display,
	guint * pwidth, guint * pheight);
typedef GstMfxWindow *(*GstMfxDisplayCreateWindowFunc) (
	GstMfxDisplay * display, guint width, guint height);
/*typedef GstMfxTexture *(*GstMfxDisplayCreateTextureFunc) (
	GstMfxDisplay * display, GstMfxID id, guint target, guint format,
	guint width, guint height);*/

typedef guintptr(*GstMfxDisplayGetVisualIdFunc) (GstMfxDisplay * display,
	GstMfxWindow * window);
typedef guintptr(*GstMfxDisplayGetColormapFunc) (GstMfxDisplay * display,
	GstMfxWindow * window);

/**
* GST_MFX_DISPLAY_GET_CLASS_TYPE:
* @display: a #GstMfxDisplay
*
* Returns the #display class type
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_DISPLAY_GET_CLASS_TYPE
#define GST_MFX_DISPLAY_GET_CLASS_TYPE(display) \
	(GST_MFX_DISPLAY_GET_CLASS (display)->display_type)

/**
* GST_MFX_DISPLAY_NATIVE:
* @display: a #GstMfxDisplay
*
* Macro that evaluates to the native display of @display.
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_DISPLAY_NATIVE
#define GST_MFX_DISPLAY_NATIVE(display) \
	(GST_MFX_DISPLAY_GET_PRIVATE (display)->native_display)

/**
* GST_MFX_DISPLAY_VADISPLAY:
* @display_: a #GstMfxDisplay
*
* Macro that evaluates to the #VADisplay of @display_.
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_DISPLAY_VADISPLAY
#define GST_MFX_DISPLAY_VADISPLAY(display_) \
	(GST_MFX_DISPLAY_GET_PRIVATE (display_)->display)

/**
* GST_MFX_DISPLAY_VADISPLAY_TYPE:
* @display: a #GstMfxDisplay
*
* Returns the underlying VADisplay @display type
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_DISPLAY_VADISPLAY_TYPE
#define GST_MFX_DISPLAY_VADISPLAY_TYPE(display) \
	(GST_MFX_DISPLAY_GET_PRIVATE (display)->display_type)

/**
* GST_MFX_DISPLAY_CACHE:
* @display: a @GstMfxDisplay
*
* Returns the #GstMfxDisplayCache attached to the supplied @display object.
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_DISPLAY_CACHE
#define GST_MFX_DISPLAY_CACHE(display) \
	(GST_MFX_DISPLAY_GET_PRIVATE (display)->cache)

struct _GstMfxDisplayPrivate
{
	GstMfxDisplay *parent;
	GRecMutex mutex;
	GstMfxDisplayCache *cache;
	GstMfxDisplayType display_type;
	gchar *display_name;
	VADisplay display;
	gpointer native_display;
	guint width;
	guint height;
	guint width_mm;
	guint height_mm;
	guint par_n;
	guint par_d;
	gchar *vendor_string;
	guint use_foreign_display : 1;
};

/**
* GstMfxDisplay:
*
* Base class for VA displays.
*/
struct _GstMfxDisplay
{
	/*< private >*/
	GstMfxMiniObject parent_instance;

	GstMfxDisplayPrivate priv;
};

/**
* GstMfxDisplayClass:
* @open_display: virtual function to open a display
* @close_display: virtual function to close a display
* @lock: (optional) virtual function to lock a display
* @unlock: (optional) virtual function to unlock a display
* @sync: (optional) virtual function to sync a display
* @flush: (optional) virtual function to flush pending requests of a display
* @get_display: virtual function to retrieve the #GstMfxDisplayInfo
* @get_size: virtual function to retrieve the display dimensions, in pixels
* @get_size_mm: virtual function to retrieve the display dimensions, in millimeters
* @get_visual_id: (optional) virtual function to retrieve the window visual id
* @get_colormap: (optional) virtual function to retrieve the window colormap
* @create_window: (optional) virtual function to create a window
* @create_texture: (optional) virtual function to create a texture
*
* Base class for VA displays.
*/
struct _GstMfxDisplayClass
{
	/*< private >*/
	GstMfxMiniObjectClass parent_class;

	/*< protected >*/
	guint display_type;

	/*< public >*/
	GstMfxDisplayInitFunc init;
	GstMfxDisplayBindFunc bind_display;
	GstMfxDisplayOpenFunc open_display;
	GstMfxDisplayCloseFunc close_display;
	GstMfxDisplayLockFunc lock;
	GstMfxDisplayUnlockFunc unlock;
	GstMfxDisplaySyncFunc sync;
	GstMfxDisplayFlushFunc flush;
	GstMfxDisplayGetInfoFunc get_display;
	GstMfxDisplayGetSizeFunc get_size;
	GstMfxDisplayGetSizeMFunc get_size_mm;
	GstMfxDisplayGetVisualIdFunc get_visual_id;
	GstMfxDisplayGetColormapFunc get_colormap;
	GstMfxDisplayCreateWindowFunc create_window;
	//GstMfxDisplayCreateTextureFunc create_texture;
};

/* Initialization types */
enum _GstMfxDisplayInitType
{
	GST_MFX_DISPLAY_INIT_FROM_DISPLAY_NAME = 1,
	GST_MFX_DISPLAY_INIT_FROM_NATIVE_DISPLAY,
	GST_MFX_DISPLAY_INIT_FROM_VA_DISPLAY
};

void
gst_mfx_display_class_init(GstMfxDisplayClass * klass);

GstMfxDisplay *
gst_mfx_display_new(const GstMfxDisplayClass * klass,
	GstMfxDisplayInitType init_type, gpointer init_value);

#define gst_mfx_display_ref_internal(display) \
	((gpointer)gst_mfx_mini_object_ref(GST_MFX_MINI_OBJECT(display)))

#define gst_mfx_display_unref_internal(display) \
	gst_mfx_mini_object_unref(GST_MFX_MINI_OBJECT(display))

#define gst_mfx_display_replace_internal(old_display_ptr, new_display) \
	gst_mfx_mini_object_replace((GstMfxMiniObject **)(old_display_ptr), \
	GST_MFX_MINI_OBJECT(new_display))

#undef  gst_mfx_display_ref
#define gst_mfx_display_ref(display) \
	gst_mfx_display_ref_internal((display))

#undef  gst_mfx_display_unref
#define gst_mfx_display_unref(display) \
	gst_mfx_display_unref_internal((display))

#undef  gst_mfx_display_replace
#define gst_mfx_display_replace(old_display_ptr, new_display) \
	gst_mfx_display_replace_internal((old_display_ptr), (new_display))

G_END_DECLS

#endif /* GST_MFX_DISPLAY_PRIV_H */
