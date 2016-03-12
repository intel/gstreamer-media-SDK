#ifndef GST_MFX_FILTER_H
#define GST_MFX_FILTER_H

#include "gstmfxsurfaceproxy.h"
#include "gstmfxcontext.h"
#include "video-utils.h"

G_BEGIN_DECLS

typedef struct _GstMfxFilter                  GstMfxFilter;

/**
* GstMfxFilterStatus:
* @GST_MFX_FILTER_STATUS_SUCCESS: Success.
* @GST_MFX_FILTER_STATUS_ERROR_ALLOCATION_FAILED: No memory left.
* @GST_MFX_FILTER_STATUS_ERROR_OPERATION_FAILED: Operation failed.
* @GST_MFX_FILTER_STATUS_ERROR_INVALID_PARAMETER: Invalid parameter.
* @GST_MFX_FILTER_STATUS_ERROR_UNSUPPORTED_OPERATION: Unsupported operation.
* @GST_MFX_FILTER_STATUS_ERROR_UNSUPPORTED_FORMAT: Unsupported target format.
*
* Video processing status for gst_mfx_filter_process().
*/
typedef enum {
	GST_MFX_FILTER_STATUS_SUCCESS = 0,
	GST_MFX_FILTER_STATUS_ERROR_ALLOCATION_FAILED,
	GST_MFX_FILTER_STATUS_ERROR_OPERATION_FAILED,
	GST_MFX_FILTER_STATUS_ERROR_INVALID_PARAMETER,
	GST_MFX_FILTER_STATUS_ERROR_UNSUPPORTED_OPERATION,
	GST_MFX_FILTER_STATUS_ERROR_UNSUPPORTED_FORMAT,
} GstMfxFilterStatus;

GstMfxFilter *
gst_mfx_filter_new(GstMfxContext * context, mfxSession * session);

GstMfxFilter *
gst_mfx_filter_ref(GstMfxFilter * filter);

void
gst_mfx_filter_unref(GstMfxFilter * filter);

void
gst_mfx_filter_replace(GstMfxFilter ** old_filter_ptr,
	GstMfxFilter * new_filter);

gboolean
gst_mfx_filter_initialize(GstMfxFilter * filter);

GstMfxFilterStatus
gst_mfx_filter_process(GstMfxFilter * filter, GstMfxSurfaceProxy *proxy,
	GstMfxSurfaceProxy ** out_proxy);

void
gst_mfx_filter_set_request(GstMfxFilter * filter,
    mfxFrameAllocRequest * request, guint flags);

GstMfxSurfacePool *
gst_mfx_filter_get_pool(GstMfxFilter * filter, guint flags);

void
gst_mfx_filter_set_format(GstMfxFilter * filter, mfxU32 fourcc);

void
gst_mfx_filter_set_size(GstMfxFilter * filter, mfxU16 width, mfxU16 height);

gboolean
gst_mfx_filter_set_cropping_rectangle(GstMfxFilter * filter,
	const GstMfxRectangle * rect);

gboolean
gst_mfx_filter_set_denoising_level(GstMfxFilter * filter, gfloat level);

gboolean
gst_mfx_filter_set_sharpening_level(GstMfxFilter * filter, gfloat level);

gboolean
gst_mfx_filter_set_hue(GstMfxFilter * filter, gfloat value);

gboolean
gst_mfx_filter_set_saturation(GstMfxFilter * filter, gfloat value);

gboolean
gst_mfx_filter_set_brightness(GstMfxFilter * filter, gfloat value);

gboolean
gst_mfx_filter_set_contrast(GstMfxFilter * filter, gfloat value);


#endif /* GST_MFX_FILTER_H */
