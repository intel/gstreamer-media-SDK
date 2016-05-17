#ifndef GST_MFXSINK_H
#define GST_MFXSINK_H

#include "gstmfxpluginbase.h"
#include "gstmfxwindow.h"
#include "gstmfxpluginutil.h"
#include "gstmfxdisplay.h"

G_BEGIN_DECLS

#define GST_TYPE_MFXSINK \
	(gst_mfxsink_get_type ())
#define GST_MFXSINK_CAST(obj) \
	((GstMfxSink *)(obj))
#define GST_MFXSINK(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MFXSINK, GstMfxSink))
#define GST_MFXSINK_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MFXSINK, GstMfxSinkClass))
#define GST_IS_MFXSINK(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MFXSINK))
#define GST_IS_MFXSINK_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MFXSINK))
#define GST_MFXSINK_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_MFXSINK, GstMfxSinkClass))

typedef struct _GstMfxSink                    GstMfxSink;
typedef struct _GstMfxSinkClass               GstMfxSinkClass;
typedef struct _GstMfxSinkBackend             GstMfxSinkBackend;

typedef gboolean(*GstMfxSinkCreateWindowFunc) (GstMfxSink * sink,
	guint width, guint height);
typedef gboolean(*GstMfxSinkHandleEventsFunc) (GstMfxSink * sink);
typedef gboolean(*GstMfxSinkPreStartEventThreadFunc) (GstMfxSink * sink);
typedef gboolean(*GstMfxSinkPreStopEventThreadFunc) (GstMfxSink * sink);

typedef enum {
	GST_MFX_GLAPI_OPENGL = 0,
	GST_MFX_GLAPI_GLES2 = 2,
} GstMfxGLAPI;

struct _GstMfxSinkBackend
{
	GstMfxSinkCreateWindowFunc create_window;

	/* Event threads handling */
	GstMfxSinkHandleEventsFunc handle_events;
	GstMfxSinkPreStartEventThreadFunc pre_start_event_thread;
	GstMfxSinkPreStopEventThreadFunc pre_stop_event_thread;
};

struct _GstMfxSink
{
	/*< private >*/
	GstMfxPluginBase parent_instance;

	const GstMfxSinkBackend *backend;

	GstCaps *caps;
	GstMfxWindow *window;
	guint window_width;
	guint window_height;
	GstBuffer *video_buffer;
	guint video_width;
	guint video_height;
	gint video_par_n;
	gint video_par_d;
	GstVideoInfo video_info;
	GstMfxRectangle display_rect;
	GThread *event_thread;
	volatile gboolean event_thread_cancel;

    GstMfxDisplay *display;
	GstMfxDisplayType display_type;
	GstMfxDisplayType display_type_req;
	gchar *display_name;

	GstMfxGLAPI gl_api;

	guint handle_events : 1;
	guint fullscreen : 1;
	guint keep_aspect : 1;
	guint signal_handoffs : 1;
};

struct _GstMfxSinkClass
{
	/*< private >*/
	GstMfxPluginBaseClass parent_class;
};

GType
gst_mfxsink_get_type(void);

G_END_DECLS


#endif /* GST_MFXSINK_H */
