#ifndef __GST_MFX_VIDEO_BUFFER_POOL_H__
#define __GST_MFX_VIDEO_BUFFER_POOL_H__

#include <gst/video/gstvideopool.h>
#include "gstmfxdisplay.h"
#include "gstmfxtaskaggregator.h"

G_BEGIN_DECLS

typedef struct _GstMfxVideoBufferPool GstMfxVideoBufferPool;
typedef struct _GstMfxVideoBufferPoolClass GstMfxVideoBufferPoolClass;
typedef struct _GstMfxVideoBufferPoolPrivate GstMfxVideoBufferPoolPrivate;

#define GST_MFX_TYPE_VIDEO_BUFFER_POOL \
	(gst_mfx_video_buffer_pool_get_type ())
#define GST_MFX_VIDEO_BUFFER_POOL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_MFX_TYPE_VIDEO_BUFFER_POOL, \
	GstMfxVideoBufferPool))
#define GST_MFX_VIDEO_BUFFER_POOL_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), GST_MFX_TYPE_VIDEO_BUFFER_POOL, \
	GstMfxVideoBufferPoolClass))
#define GST_MFX_IS_VIDEO_BUFFER_POOL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_MFX_TYPE_VIDEO_BUFFER_POOL))
#define GST_MFX_IS_VIDEO_BUFFER_POOL_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), GST_MFX_TYPE_VIDEO_BUFFER_POOL))


#define GST_BUFFER_POOL_OPTION_MFX_VIDEO_META \
	"GstBufferPoolOptionMfxVideoMeta"
		
#ifndef GST_BUFFER_POOL_OPTION_DMABUF_MEMORY
#define GST_BUFFER_POOL_OPTION_DMABUF_MEMORY \
	"GstBufferPoolOptionDMABUFMemory"
#endif

struct _GstMfxVideoBufferPool
{
	GstBufferPool bufferpool;

	GstMfxVideoBufferPoolPrivate *priv;
};

struct _GstMfxVideoBufferPoolClass
{
	GstBufferPoolClass parent_instance;
};

GType gst_mfx_video_buffer_pool_get_type(void);

GstBufferPool *
gst_mfx_video_buffer_pool_new(GstMfxDisplay * display, gboolean mapped);

G_END_DECLS

#endif /* __GST_MFX_VIDEO_BUFFER_POOL_H__ */
