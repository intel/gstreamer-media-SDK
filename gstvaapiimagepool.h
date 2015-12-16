#ifndef GST_VAAPI_IMAGE_POOL_H
#define GST_VAAPI_IMAGE_POOL_H

#include "gstvaapiimage.h"
#include "gstmfxobjectpool.h"
#include "video-utils.h"

G_BEGIN_DECLS

#define GST_VAAPI_IMAGE_POOL(obj) \
    ((GstVaapiImagePool *)(obj))

typedef struct _GstVaapiImagePool GstVaapiImagePool;

GstMfxObjectPool *
gst_vaapi_image_pool_new (VADisplay display, const GstVideoInfo * vip);

G_END_DECLS

#endif /* GST_VAAPI_IMAGE_POOL_H */
