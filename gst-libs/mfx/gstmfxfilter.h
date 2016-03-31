#ifndef GST_MFX_FILTER_H
#define GST_MFX_FILTER_H

#include "gstmfxsurfaceproxy.h"
#include "gstmfxtaskaggregator.h"
#include "video-utils.h"

G_BEGIN_DECLS

typedef struct _GstMfxFilter                    GstMfxFilter;

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

/**
 * GstMfxFilterOp:
 * GST_MFX_FILTER_OP_NONE: No operation.
 * GST_MFX_FILTER_OP_PIXEL_CONVERSION: Pixel conversion operation.
 * GST_MFX_FILTER_OP_SCALE: Scaling operation.
 * GST_MFX_FILTER_OP_CROP: Cropping operation.
 * GST_MFX_FILTER_OP_DEINTERLACING: Deinterlacing operation.
 * GST_MFX_FILTER_OP_DENOISE: Denoise operation.
 * GST_MFX_FILTER_OP_DETAIL: Detail operation.
 * GST_MFX_FILTER_OP_FRAMERATE_CONVERSION: Frame rate conversion operation.
 * GST_MFX_FILTER_OP_BRIGHTNESS: Brightness operation.
 * GST_MFX_FILTER_OP_SATURATION: Saturation operation.
 * GST_MFX_FILTER_OP_HUE: Hue operation.
 * GST_MFX_FILTER_OP_CONTRAST: Contrast operation.
 * GST_MFX_FILTER_OP_FIELD_PROCESSING: Field processing operation.
 * GST_MFX_FILTER_OP_IMAGE_STABILIZATION: Image stabilization operation.
 * GST_MFX_FILTER_OP_ROTATION: Rotation operation.
 */

typedef enum {
    GST_MFX_FILTER_OP_NONE = 0,
    GST_MFX_FILTER_OP_FORMAT,
    GST_MFX_FILTER_OP_SCALE,
    GST_MFX_FILTER_OP_CROP,
    GST_MFX_FILTER_OP_DEINTERLACING,
    GST_MFX_FILTER_OP_DENOISE,
    GST_MFX_FILTER_OP_DETAIL,
    GST_MFX_FILTER_OP_FRAMERATE_CONVERSION,
    GST_MFX_FILTER_OP_BRIGHTNESS,
    GST_MFX_FILTER_OP_SATURATION,
    GST_MFX_FILTER_OP_HUE,
    GST_MFX_FILTER_OP_CONTRAST,
    GST_MFX_FILTER_OP_FIELD_PROCESSING,
    GST_MFX_FILTER_OP_IMAGE_STABILIZATION,
    GST_MFX_FILTER_OP_ROTATION,
} GstMfxFilterOp;

/*
 * GstMfxFilterType:
 * GST_MFX_FILTER_NONE: No filter.
 * GST_MFX_FILTER_DEINTERLACING: Deinterlacing filter.
 * GST_MFX_FILTER_DENOISE: Denoise filter.
 * GST_MFX_FILTER_DETAIL: Detail filter.
 * GST_MFX_FILTER_FRAMERATE_CONVERSION: Framerate conversion filter.
 * GST_MFX_FILTER_PROCAMP: ProcAmp filter.
 * GST_MFX_FILTER_FIELD_PROCESSING: Field processing filter.
 * GST_MFX_FILTER_IMAGE_STABILIZATION: Image stabilization filter.
 * GST_MFX_FILTER_ROTATION: Rotation filter.
 */

typedef enum {
    GST_MFX_FILTER_NONE = 0,
    GST_MFX_FILTER_DEINTERLACING = (1 << GST_MFX_FILTER_OP_DEINTERLACING),
    GST_MFX_FILTER_DENOISE = (1 << GST_MFX_FILTER_OP_DENOISE),
    GST_MFX_FILTER_DETAIL = (1 << GST_MFX_FILTER_OP_DETAIL),
    GST_MFX_FILTER_FRAMERATE_CONVERSION = (1 << GST_MFX_FILTER_OP_FRAMERATE_CONVERSION),
    GST_MFX_FILTER_PROCAMP = (1 << GST_MFX_FILTER_OP_BRIGHTNESS),
    GST_MFX_FILTER_FIELD_PROCESSING = (1 << GST_MFX_FILTER_OP_FIELD_PROCESSING),
    GST_MFX_FILTER_IMAGE_STABILIZATION = (1 << GST_MFX_FILTER_OP_IMAGE_STABILIZATION),
    GST_MFX_FILTER_ROTATION = (1 << GST_MFX_FILTER_OP_ROTATION),
} GstMfxFilterType;

typedef enum {
	GST_MFX_DEINTERLACE_MODE_NONE,
	GST_MFX_DEINTERLACE_MODE_BOB,
	GST_MFX_DEINTERLACE_MODE_ADVANCED,
	GST_MFX_DEINTERLACE_MODE_ADVANCED_NO_REF,
} GstMfxDeinterlaceMode;


GstMfxFilter *
gst_mfx_filter_new(GstMfxTaskAggregator * aggregator);

GstMfxFilter *
gst_mfx_filter_new_with_session(GstMfxTaskAggregator * aggregator, mfxSession * session);

GstMfxFilter *
gst_mfx_filter_ref(GstMfxFilter * filter);

void
gst_mfx_filter_unref(GstMfxFilter * filter);

void
gst_mfx_filter_replace(GstMfxFilter ** old_filter_ptr,
	GstMfxFilter * new_filter);

gboolean
gst_mfx_filter_start(GstMfxFilter * filter);

GstMfxFilterStatus
gst_mfx_filter_process(GstMfxFilter * filter, GstMfxSurfaceProxy *proxy,
	GstMfxSurfaceProxy ** out_proxy);

gboolean
gst_mfx_filter_has_filter(GstMfxFilter * filter, guint flags);

GstMfxSurfacePool *
gst_mfx_filter_get_pool(GstMfxFilter * filter, guint flags);

void
gst_mfx_filter_set_request(GstMfxFilter * filter,
    mfxFrameAllocRequest * request, guint flags);

void
gst_mfx_filter_set_frame_info(GstMfxFilter * filter, GstVideoInfo * info);

gboolean
gst_mfx_filter_set_format(GstMfxFilter * filter, GstVideoFormat fmt);

gboolean
gst_mfx_filter_set_denoising_level(GstMfxFilter * filter, guint level);

gboolean
gst_mfx_filter_set_detail_level(GstMfxFilter * filter, guint level);

gboolean
gst_mfx_filter_set_hue(GstMfxFilter * filter, gfloat value);

gboolean
gst_mfx_filter_set_saturation(GstMfxFilter * filter, gfloat value);

gboolean
gst_mfx_filter_set_brightness(GstMfxFilter * filter, gfloat value);

gboolean
gst_mfx_filter_set_contrast(GstMfxFilter * filter, gfloat value);

gboolean
gst_mfx_filter_set_rotation(GstMfxFilter * filter, GstMfxRotation angle);

#endif /* GST_MFX_FILTER_H */
