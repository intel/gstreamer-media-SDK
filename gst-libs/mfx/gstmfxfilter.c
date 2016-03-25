#include "sysdeps.h"
#include "gstmfxfilter.h"
#include "gstmfxtaskaggregator.h"
#include "gstmfxtask.h"
#include "gstmfxsurfacepool.h"
#include "gstmfxsurfaceproxy.h"

#include <mfxvideo.h>

#define DEBUG 1
#include "gstmfxdebug.h"

#define GST_MFX_FILTER(obj) \
	((GstMfxFilter *)(obj))

#define MSDK_ALIGN16(X) (((mfxU16)((X)+15)) & (~ (mfxU16)15))

typedef struct _GstMfxFilterOpData GstMfxFilterOpData;

typedef struct
{
    GstMfxFilterType type;
    mfxU32 filter;
    gchar desc[64];
} GstMfxFilterMap;

struct _GstMfxFilterOpData
{
    GstMfxFilterType type;
    gpointer filter;
    gsize size;
};

struct _GstMfxFilter
{
	/*< private >*/
	GstMfxMiniObject parent_instance;
	GstMfxTaskAggregator *aggregator;
	GstMfxTask *vpp[2];
	GstMfxSurfacePool *vpp_pool[2];
	mfxSession session;
	mfxVideoParam params;
	mfxFrameInfo *frame_info[2];
    mfxFrameAllocRequest *vpp_request[2];

	gboolean internal_session;

	/* VPP output parameters */
	mfxU32 fourcc;
	mfxU16 width;
	mfxU16 height;

    /* FilterType */
    guint supported_filter;
    guint filter_op;
    GPtrArray *filter_op_data;

	mfxExtBuffer **ext_buffer;
    mfxExtVPPDoUse vpp_use;
};

static const GstMfxFilterMap filter_map[] = {
    { GST_MFX_FILTER_DEINTERLACING, MFX_EXTBUFF_VPP_DEINTERLACING, "Deinterlacing filter" },
    { GST_MFX_FILTER_PROCAMP, MFX_EXTBUFF_VPP_PROCAMP, "ProcAmp filter" },
    { GST_MFX_FILTER_DETAIL, MFX_EXTBUFF_VPP_DETAIL, "Detail filter" },
    { GST_MFX_FILTER_DENOISE, MFX_EXTBUFF_VPP_DENOISE, "Denoise filter" },
    { GST_MFX_FILTER_FRAMERATE_CONVERSION, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION, "Framerate conversion filter" },
    { GST_MFX_FILTER_FIELD_PROCESSING, MFX_EXTBUFF_VPP_FIELD_PROCESSING, "Field processing filter" },
    { GST_MFX_FILTER_IMAGE_STABILIZATION, MFX_EXTBUFF_VPP_IMAGE_STABILIZATION, "Image stabilization filter" },
    { GST_MFX_FILTER_ROTATION, MFX_EXTBUFF_VPP_ROTATION, "Rotation filter" },
    {0, }
};

void
gst_mfx_filter_set_format(GstMfxFilter * filter, mfxU32 fourcc)
{
    g_return_if_fail (filter != NULL);

	filter->fourcc = fourcc;
}

void
gst_mfx_filter_set_size(GstMfxFilter * filter, mfxU16 width, mfxU16 height)
{
    g_return_if_fail (filter != NULL);

	filter->width = width;
	filter->height = height;
}

void
gst_mfx_filter_set_request(GstMfxFilter * filter,
    mfxFrameAllocRequest * request, guint flags)
{
    filter->vpp_request[flags & GST_MFX_TASK_VPP_OUT] = (mfxFrameAllocRequest *)
        g_slice_copy(sizeof(mfxFrameAllocRequest), request);

    filter->frame_info[flags & GST_MFX_TASK_VPP_OUT] =
        &filter->vpp_request[flags & GST_MFX_TASK_VPP_OUT]->Info;

    if (flags & GST_MFX_TASK_VPP_IN)
        filter->vpp_request[0]->Type |= MFX_MEMTYPE_FROM_VPPIN;
    else
        filter->vpp_request[1]->Type |= MFX_MEMTYPE_FROM_VPPOUT;
}

static void check_supported_filter(GstMfxFilter *filter)
{
    mfxVideoParam param;
    mfxExtVPPDoUse vpp_use;
    mfxExtBuffer *extbuf[1];
    mfxStatus sts;
    const GstMfxFilterMap *m;

    filter->supported_filter = GST_MFX_FILTER_NONE;
    memset(&vpp_use, 0, sizeof(mfxExtVPPDoUse));

    vpp_use.NumAlg = 1;
    vpp_use.AlgList = g_slice_alloc(sizeof(mfxExtVPPDoUse));
    vpp_use.Header.BufferId = MFX_EXTBUFF_VPP_DOUSE;
    param.NumExtParam = 1;

    extbuf[0] = (mfxExtBuffer *)&vpp_use;
    param.ExtParam = (mfxExtBuffer **)&extbuf[0];


    /* check filters */
    for(m = filter_map; m->type; m++) {
        vpp_use.AlgList[0] = m->filter;
        sts = MFXVideoVPP_Query(filter->session, NULL, &param);
        if (MFX_ERR_NONE == sts)
            filter->supported_filter |= m->type;
        else
            g_printf("%s is not supported in this platform!\n", m->desc);
    }

    /* Release the resource */
    g_slice_free(mfxExtVPPDoUse, vpp_use.AlgList);
}

static gboolean
init_filters(GstMfxFilter * filter)
{
    guint i;
    GstMfxFilterOpData *op;
    mfxExtBuffer *ext_buf;
    memset(&filter->vpp_use, 0, sizeof(mfxExtVPPDoUse));

    filter->vpp_use.Header.BufferId = MFX_EXTBUFF_VPP_DOUSE;
    filter->vpp_use.Header.BufferSz = sizeof(mfxExtVPPDoUse);
    filter->vpp_use.NumAlg = filter->filter_op_data->len;
	filter->vpp_use.AlgList = g_slice_alloc(
        filter->vpp_use.NumAlg * sizeof(mfxU32));
	if (!filter->vpp_use.AlgList)
		return FALSE;

    filter->ext_buffer = g_slice_alloc((filter->vpp_use.NumAlg + 1)*sizeof(mfxExtBuffer*));
    if (!filter->ext_buffer)
        return FALSE;

    for(i = 0; i<filter->filter_op_data->len; i++)
    {
        op = (GstMfxFilterOpData *)g_ptr_array_index(filter->filter_op_data, i);
        ext_buf = (mfxExtBuffer *)op->filter;
        filter->vpp_use.AlgList[i] = ext_buf->BufferId;
        filter->ext_buffer[i+1] = (mfxExtBuffer *)op->filter;
    }

    filter->ext_buffer[0] = (mfxExtBuffer*)&filter->vpp_use;

    filter->params.NumExtParam = filter->vpp_use.NumAlg+1;
	filter->params.ExtParam = (mfxExtBuffer **) &filter->ext_buffer[0];
}

static gboolean
init_params(GstMfxFilter * filter)
{
    if (!filter->frame_info[0])
        return FALSE;

    memcpy(&filter->params.vpp.In, filter->frame_info[0],
            sizeof(mfxFrameInfo));

    memcpy(&filter->params.vpp.Out,
        filter->frame_info[1] ? filter->frame_info[1] : &filter->params.vpp.In,
        sizeof(mfxFrameInfo));

    if (filter->fourcc) {
        filter->params.vpp.Out.FourCC = filter->fourcc;
    }
    if (filter->width) {
        filter->params.vpp.Out.CropW = filter->width;
        filter->params.vpp.Out.Width = MSDK_ALIGN16(filter->width);
    }
    if (filter->height) {
        filter->params.vpp.Out.CropH = filter->height;
        filter->params.vpp.Out.Height = MSDK_ALIGN16(filter->height);
    }

    return TRUE;
}

gboolean
gst_mfx_filter_start(GstMfxFilter * filter)
{
    mfxFrameAllocRequest vpp_request[2];
    mfxFrameAllocResponse response;
    mfxStatus sts;
    guint i;

    if (!init_params(filter))
        return FALSE;

    sts = MFXVideoVPP_QueryIOSurf(filter->session, &filter->params, &vpp_request);
    if (sts < 0) {
        GST_ERROR("Unable to query VPP allocation request %d", sts);
        return FALSE;
    }

    for (i = 0; i < 2; i++) {
        /* No need for input VPP pool when no input alloc request is set */
        if (!filter->vpp_request[0])
            continue;

        GstMfxTaskType type = i == 0 ? GST_MFX_TASK_VPP_IN : GST_MFX_TASK_VPP_OUT;

        filter->vpp[i] = gst_mfx_task_aggregator_find_task(filter->aggregator,
            &filter->session, type);
        if (!filter->vpp[i]) {
            if (!filter->internal_session)
                filter->vpp[i] = gst_mfx_task_new_with_session(
                    filter->aggregator, &filter->session, type);
            else
                filter->vpp[i] = gst_mfx_task_new(filter->aggregator, type);
        }

        if (!filter->vpp_request[i]) {
            filter->vpp_request[i] = (mfxFrameAllocRequest *)
                g_slice_copy(sizeof(mfxFrameAllocRequest), &vpp_request[i]);
        }
        else {
            filter->vpp_request[i]->NumFrameSuggested +=
                vpp_request[i].NumFrameSuggested;
            filter->vpp_request[i]->NumFrameMin =
                filter->vpp_request[i]->NumFrameSuggested;
        }

        filter->vpp_request[i]->Type |= MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;

        sts = gst_mfx_task_frame_alloc(filter->vpp[i], filter->vpp_request[i],
                &response);
        if (MFX_ERR_NONE != sts)
            return FALSE;

        filter->vpp_pool[i] = gst_mfx_surface_pool_new(filter->vpp[i]);
        if (!filter->vpp_pool[i])
            return FALSE;
    }

    //Check supported filters
    check_supported_filter(filter);

    init_filters(filter);

    sts = MFXVideoVPP_Init(filter->session, &filter->params);
    if (sts < 0) {
		GST_ERROR("Error initializing MFX VPP %d", sts);
		return FALSE;
    }
    return TRUE;
}

static GstMfxFilterOpData *
find_filter_op_data(GstMfxFilter *filter, GstMfxFilterType type)
{
    guint i;
    GstMfxFilterOpData *op;
    for(i=0;i<filter->filter_op_data->len;i++)
    {
        op = (GstMfxFilterOpData *)g_ptr_array_index(filter->filter_op_data, i);
        if(type == op->type)
            return op;
    }

    return NULL;
}

static void
free_filter_op_data(gpointer data)
{
    GstMfxFilterOpData *op = (GstMfxFilterOpData *)data;
    g_slice_free1(op->size, op->filter);
    g_slice_free(GstMfxFilterOpData, op);
}

static gboolean
gst_mfx_filter_init(GstMfxFilter * filter,
	GstMfxTaskAggregator * aggregator, mfxSession * session)
{
	mfxFrameAllocResponse response;
	mfxStatus sts;

	memset(&filter->params, 0, sizeof (filter->params));

	filter->params.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY |
        MFX_IOPATTERN_OUT_VIDEO_MEMORY;
	filter->aggregator = gst_mfx_task_aggregator_ref(aggregator);
	filter->internal_session = session ? FALSE : TRUE;
	if (!filter->internal_session)
        filter->session = *session;

    //Initialize the array of operation data
    filter->filter_op_data =
    g_ptr_array_new_with_free_func(free_filter_op_data);

    //Initialize the filter flag
    filter->filter_op = GST_MFX_FILTER_NONE;

	return TRUE;
}

gboolean
gst_mfx_filter_has_filter(GstMfxFilter * filter, guint flags)
{
    g_return_val_if_fail(filter != NULL, FALSE);
    return (filter->supported_filter & flags) != 0;
}

static void
gst_mfx_filter_finalize(GstMfxFilter * filter)
{
    guint i;

	for (i = 0; i < 2; i++) {
        if (!filter->vpp_request[0])
            continue;

        gst_mfx_task_unref(filter->vpp[i]);
        gst_mfx_surface_pool_unref(filter->vpp_pool[i]);
	}

    /* Free allocated memory for filters */
    g_slice_free1((sizeof(mfxU32) * filter->vpp_use.NumAlg),
            filter->vpp_use.AlgList);

    g_slice_free1((sizeof(mfxExtBuffer *) * filter->params.NumExtParam),
            filter->ext_buffer);

    g_ptr_array_free(filter->filter_op_data, TRUE);

    if (filter->internal_session)
        MFXVideoVPP_Close(filter->session);
	gst_mfx_task_aggregator_unref(filter->aggregator);
}

static inline const GstMfxMiniObjectClass *
gst_mfx_filter_class(void)
{
	static const GstMfxMiniObjectClass GstMfxFilterClass = {
		sizeof (GstMfxFilter),
		(GDestroyNotify)gst_mfx_filter_finalize
	};
	return &GstMfxFilterClass;
}

GstMfxFilter *
gst_mfx_filter_new(GstMfxTaskAggregator * aggregator)
{
	GstMfxFilter *filter;

	g_return_val_if_fail(aggregator != NULL, NULL);

	filter = (GstMfxFilter *)
		gst_mfx_mini_object_new0(gst_mfx_filter_class());
	if (!filter)
		return NULL;

	if (!gst_mfx_filter_init(filter, aggregator, NULL))
		goto error;
	return filter;

error:
	gst_mfx_filter_unref(filter);
	return NULL;
}

GstMfxFilter *
gst_mfx_filter_new_with_session(GstMfxTaskAggregator * aggregator, mfxSession * session)
{
	GstMfxFilter *filter;

	g_return_val_if_fail(aggregator != NULL, NULL);
	g_return_val_if_fail(session != NULL, NULL);

	filter = (GstMfxFilter *)
		gst_mfx_mini_object_new0(gst_mfx_filter_class());
	if (!filter)
		return NULL;

	if (!gst_mfx_filter_init(filter, aggregator, session))
		goto error;
	return filter;

error:
	gst_mfx_filter_unref(filter);
	return NULL;
}

GstMfxFilter *
gst_mfx_filter_ref(GstMfxFilter * filter)
{
	g_return_val_if_fail(filter != NULL, NULL);

	return
		GST_MFX_FILTER(gst_mfx_mini_object_ref(GST_MFX_MINI_OBJECT (filter)));
}

void
gst_mfx_filter_unref(GstMfxFilter * filter)
{
	g_return_if_fail(filter != NULL);

	gst_mfx_mini_object_unref(GST_MFX_MINI_OBJECT(filter));
}


void
gst_mfx_filter_replace(GstMfxFilter ** old_filter_ptr,
    GstMfxFilter * new_filter)
{
	g_return_if_fail(old_filter_ptr != NULL);

	gst_mfx_mini_object_replace((GstMfxMiniObject **)old_filter_ptr,
		GST_MFX_MINI_OBJECT(new_filter));
}

GstMfxSurfacePool *
gst_mfx_filter_get_pool(GstMfxFilter * filter, guint flags)
{
    return filter->vpp_pool[flags & GST_MFX_TASK_VPP_OUT];
}

GstMfxFilterStatus
gst_mfx_filter_process(GstMfxFilter * filter, GstMfxSurfaceProxy *proxy,
	GstMfxSurfaceProxy ** out_proxy)
{
	mfxFrameSurface1 *insurf, *outsurf = NULL;
	mfxSyncPoint syncp;
	mfxStatus sts = MFX_ERR_NONE;

	do {
		*out_proxy = gst_mfx_surface_proxy_new_from_pool(filter->vpp_pool[1]);
		if (!*out_proxy)
			return GST_MFX_FILTER_STATUS_ERROR_ALLOCATION_FAILED;

		insurf = gst_mfx_surface_proxy_get_frame_surface(proxy);
		outsurf = gst_mfx_surface_proxy_get_frame_surface(*out_proxy);
		sts = MFXVideoVPP_RunFrameVPPAsync(filter->session, insurf, outsurf, NULL, &syncp);
		if (MFX_WRN_DEVICE_BUSY == sts)
			g_usleep(500);
	} while (MFX_WRN_DEVICE_BUSY == sts);

	if (MFX_ERR_NONE != sts) {
		GST_ERROR("Error during MFX filter process.");
		return GST_MFX_FILTER_STATUS_ERROR_OPERATION_FAILED;
	}

	if (syncp) {
		do {
			sts = MFXVideoCORE_SyncOperation(filter->session, syncp, 1000);
		} while (MFX_WRN_IN_EXECUTION == sts);

		*out_proxy = gst_mfx_surface_pool_find_proxy(filter->vpp_pool[1], outsurf);
	}

	return GST_MFX_FILTER_STATUS_SUCCESS;
}

static gpointer init_procamp_default()
{
    mfxExtVPPProcAmp *ext_procamp;
    ext_procamp = g_slice_alloc0(sizeof(mfxExtVPPProcAmp));
    if (!ext_procamp)
        return NULL;
    ext_procamp->Header.BufferId = MFX_EXTBUFF_VPP_PROCAMP;
    ext_procamp->Header.BufferSz = sizeof(mfxExtVPPProcAmp);
    ext_procamp->Brightness = 0.0;
    ext_procamp->Contrast = 1.0;
    ext_procamp->Hue = 0.0;
    ext_procamp->Saturation = 1.0;
    return ext_procamp;
}

static gpointer init_denoise_default()
{
    mfxExtVPPDenoise *ext_denoise;
    ext_denoise = g_slice_alloc0(sizeof(mfxExtVPPDenoise));
    if (!ext_denoise)
        return NULL;
    ext_denoise->Header.BufferId = MFX_EXTBUFF_VPP_DENOISE;
    ext_denoise->Header.BufferSz = sizeof(mfxExtVPPDenoise);
    ext_denoise->DenoiseFactor = 0;
    return ext_denoise;
}

static gpointer init_detail_default()
{
    mfxExtVPPDetail *ext_detail;
    ext_detail = g_slice_alloc0(sizeof(mfxExtVPPDetail));
    if (!ext_detail)
        return NULL;
    ext_detail->Header.BufferId = MFX_EXTBUFF_VPP_DETAIL;
    ext_detail->Header.BufferSz = sizeof(mfxExtVPPDetail);
    ext_detail->DetailFactor = 0;
    return ext_detail;
}

static gpointer init_rotation_default()
{
    mfxExtVPPRotation *ext_rotation;
    ext_rotation = g_slice_alloc0(sizeof(mfxExtVPPRotation));
    if (!ext_rotation)
        return NULL;
    ext_rotation->Header.BufferId = MFX_EXTBUFF_VPP_ROTATION;
    ext_rotation->Header.BufferSz = sizeof(mfxExtVPPRotation);
    ext_rotation->Angle = MFX_ANGLE_0;
    return ext_rotation;
}

gboolean
gst_mfx_filter_set_saturation(GstMfxFilter * filter, gfloat value)
{
    GstMfxFilterOpData *op;
    mfxExtVPPProcAmp *ext_procamp;

    g_return_val_if_fail(filter !=  NULL, FALSE);
    g_return_val_if_fail(value <=  10.0, FALSE);
    g_return_val_if_fail(value >=  0.0, FALSE);

    op = find_filter_op_data(filter, GST_MFX_FILTER_PROCAMP);
    if( NULL == op ) {
        op = g_slice_alloc(sizeof(GstMfxFilterOpData));
        if ( NULL == op )
            return FALSE;
        op->type = GST_MFX_FILTER_PROCAMP;
        filter->filter_op |= GST_MFX_FILTER_PROCAMP;
        op->size = sizeof(mfxExtVPPProcAmp);
        op->filter = init_procamp_default();
        if( NULL == op->filter ) {
            g_slice_free(GstMfxFilterOpData, op);
            return FALSE;
        }
        g_ptr_array_add(filter->filter_op_data, op);
    }
    ext_procamp = (mfxExtVPPProcAmp *)op->filter;
    ext_procamp->Saturation = value;

    return TRUE;
}

gboolean
gst_mfx_filter_set_brightness(GstMfxFilter * filter, gfloat value)
{
    GstMfxFilterOpData *op;
    mfxExtVPPProcAmp *ext_procamp;

    g_return_val_if_fail(filter !=  NULL, FALSE);
    g_return_val_if_fail(value <=  100.0, FALSE);
    g_return_val_if_fail(value >=  -100.0, FALSE);

    op = find_filter_op_data(filter, GST_MFX_FILTER_PROCAMP);
    if( NULL == op ) {
        op = g_slice_alloc(sizeof(GstMfxFilterOpData));
        if ( NULL == op )
            return FALSE;
        op->type = GST_MFX_FILTER_PROCAMP;
        filter->filter_op |= GST_MFX_FILTER_PROCAMP;
        op->size = sizeof(mfxExtVPPProcAmp);
        op->filter = init_procamp_default();
        if( NULL == op->filter ) {
            g_slice_free(GstMfxFilterOpData, op);
            return FALSE;
        }
        g_ptr_array_add(filter->filter_op_data, op);
    }

    ext_procamp = (mfxExtVPPProcAmp *)op->filter;
    ext_procamp->Brightness = value;

    return TRUE;
}

gboolean
gst_mfx_filter_set_contrast(GstMfxFilter * filter, gfloat value)
{
    GstMfxFilterOpData *op;
    mfxExtVPPProcAmp *ext_procamp;

    g_return_val_if_fail(filter !=  NULL, FALSE);
    g_return_val_if_fail(value <=  10.0, FALSE);
    g_return_val_if_fail(value >=  0.0, FALSE);

    op = find_filter_op_data(filter, GST_MFX_FILTER_PROCAMP);
    if( NULL == op ) {
        op = g_slice_alloc(sizeof(GstMfxFilterOpData));
        if ( NULL == op )
            return FALSE;
        op->type = GST_MFX_FILTER_PROCAMP;
        filter->filter_op |= GST_MFX_FILTER_PROCAMP;
        op->size = sizeof(mfxExtVPPProcAmp);
        op->filter = init_procamp_default();
        if( NULL == op->filter ) {
            g_slice_free(GstMfxFilterOpData, op);
            return FALSE;
        }
        g_ptr_array_add(filter->filter_op_data, op);
    }

    ext_procamp = (mfxExtVPPProcAmp *)op->filter;
    ext_procamp->Contrast = value;

    return TRUE;
}

gboolean
gst_mfx_filter_set_hue(GstMfxFilter * filter, gfloat value)
{
    GstMfxFilterOpData *op;
    mfxExtVPPProcAmp *ext_procamp;

    g_return_val_if_fail(filter !=  NULL, FALSE);
    g_return_val_if_fail(value <=  180.0, FALSE);
    g_return_val_if_fail(value >=  -180.0, FALSE);

    op = find_filter_op_data(filter, GST_MFX_FILTER_PROCAMP);
    if( NULL == op ) {
        op = g_slice_alloc(sizeof(GstMfxFilterOpData));
        if ( NULL == op )
            return FALSE;
        op->type = GST_MFX_FILTER_PROCAMP;
        filter->filter_op |= GST_MFX_FILTER_PROCAMP;
        op->size = sizeof(mfxExtVPPProcAmp);
        op->filter = init_procamp_default();
        if( NULL == op->filter ) {
            g_slice_free(GstMfxFilterOpData, op);
            return FALSE;
        }
        g_ptr_array_add(filter->filter_op_data, op);
    }

    ext_procamp = (mfxExtVPPProcAmp *)op->filter;
    ext_procamp->Hue = value;

    return TRUE;
}

gboolean
gst_mfx_filter_set_denoising_level(GstMfxFilter * filter, guint level)
{
    GstMfxFilterOpData *op;
    mfxExtVPPDenoise *ext_denoise;

    g_return_val_if_fail(filter != NULL, FALSE);
    g_return_val_if_fail(level >= 0, FALSE);
    g_return_val_if_fail(level <= 100, FALSE);

    op = find_filter_op_data(filter, GST_MFX_FILTER_DENOISE);
    if ( NULL == op ) {
        op = g_slice_alloc(sizeof(GstMfxFilterOpData));
        if ( NULL == op )
            return FALSE;
        op->type = GST_MFX_FILTER_DENOISE;
        filter->filter_op |= GST_MFX_FILTER_DENOISE;
        op->size = sizeof(mfxExtVPPDenoise);
        op->filter = init_denoise_default();
        if( NULL == op->filter ) {
            g_slice_free(GstMfxFilterOpData, op);
            return FALSE;
        }
        g_ptr_array_add(filter->filter_op_data, op);
    }

    ext_denoise = (mfxExtVPPDenoise *)op->filter;
    ext_denoise->DenoiseFactor = level;

    return TRUE;
}

gboolean
gst_mfx_filter_set_detail_level(GstMfxFilter * filter, guint level)
{
    GstMfxFilterOpData *op;
    mfxExtVPPDetail *ext_detail;
    g_return_val_if_fail(filter != NULL, FALSE);
    g_return_val_if_fail(level >= 0, FALSE);
    g_return_val_if_fail(level <= 100, FALSE);

    op = find_filter_op_data(filter, GST_MFX_FILTER_DETAIL);
    if ( NULL == op ) {
        op = g_slice_alloc(sizeof(GstMfxFilterOpData));
        if ( NULL == op )
            return FALSE;
        op->type = GST_MFX_FILTER_DETAIL;
        filter->filter_op |= GST_MFX_FILTER_DETAIL;
        op->size = sizeof(mfxExtVPPDetail);
        op->filter = init_detail_default();
        if ( NULL == op->filter ) {
            g_slice_free(GstMfxFilterOpData, op);
            return FALSE;
        }
        g_ptr_array_add(filter->filter_op_data, op);
    }

    ext_detail = (mfxExtVPPDetail *)op->filter;
    ext_detail->DetailFactor = level;

    return TRUE;
}

gboolean
gst_mfx_filter_set_rotation(GstMfxFilter * filter, guint angle)
{
    GstMfxFilterOpData *op;
    mfxExtVPPRotation *ext_rotation;

    g_return_val_if_fail(filter != NULL, FALSE);
    g_return_val_if_fail((angle == MFX_ANGLE_0 ||
                angle == MFX_ANGLE_90 ||
                angle == MFX_ANGLE_180 ||
                angle == MFX_ANGLE_270), FALSE);

    op = find_filter_op_data(filter, GST_MFX_FILTER_ROTATION);
    if ( NULL == op ) {
        op = g_slice_alloc(sizeof(GstMfxFilterOpData));
        if ( NULL == op )
            return FALSE;
        op->type = GST_MFX_FILTER_ROTATION;
        filter->filter_op |= GST_MFX_FILTER_ROTATION;
        op->size = sizeof(mfxExtVPPRotation);
        op->filter = init_rotation_default();
        if ( NULL == op->filter ) {
            g_slice_free(GstMfxFilterOpData, op);
            return FALSE;
        }
        g_ptr_array_add(filter->filter_op_data, op);
    }

    ext_rotation = (mfxExtVPPRotation *)op->filter;
    ext_rotation->Angle = angle;

    return TRUE;
}
