#include "sysdeps.h"
#include "gstmfxfilter.h"
#include "gstmfxcontext.h"
#include "gstmfxtask.h"
#include "gstmfxsurfacepool.h"
#include "gstmfxsurfaceproxy.h"

#include <mfxvideo.h>

#define DEBUG 1
#include "gstmfxdebug.h"

#define GST_MFX_FILTER(obj) \
	((GstMfxFilter *)(obj))

#define MSDK_ALIGN16(X) (((mfxU16)((X)+15)) & (~ (mfxU16)15))


struct _GstMfxFilter
{
	/*< private >*/
	GstMfxMiniObject parent_instance;

	GstMfxContext *context;
	GList *vpp_ext_params;

	GstMfxTask *vpp[2];
	GstMfxSurfacePool *vpp_pool[2];

	mfxSession session;
	mfxVideoParam params;
	mfxFrameInfo *frame_info[2];
	mfxFrameAllocRequest *vpp_request[2];

	/* VPP output parameters */
	mfxU32 fourcc;
	mfxU16 width;
	mfxU16 height;
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

    //init_filters(filter);

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
gst_mfx_filter_initialize(GstMfxFilter * filter)
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

        filter->vpp[i] = gst_mfx_context_find_task(filter->context,
            &filter->session,
            i == 0 ? GST_MFX_TASK_VPP_IN : GST_MFX_TASK_VPP_OUT);
        if (!filter->vpp[i]) {
            filter->vpp[i] = gst_mfx_task_new_with_session(filter->context,
                &filter->session,
                i == 0 ? GST_MFX_TASK_VPP_IN : GST_MFX_TASK_VPP_OUT);
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

    sts = MFXVideoVPP_Init(filter->session, &filter->params);
    if (sts < 0) {
		GST_ERROR("Error initializing MFX VPP %d", sts);
		return FALSE;
	}

    return TRUE;
}

static gboolean
init_filters(GstMfxFilter * filter)
{
    mfxExtVPPDoNotUse vpp_not_used;
	//mfxExtVPPDeinterlacing  vpp_deinterlacing;

    vpp_not_used.NumAlg = 4;
	vpp_not_used.AlgList = g_slice_alloc(vpp_not_used.NumAlg * sizeof(*vpp_not_used.AlgList));
	if (!vpp_not_used.AlgList)
		return FALSE;

	vpp_not_used.AlgList[0] = MFX_EXTBUFF_VPP_DENOISE;
	vpp_not_used.AlgList[1] = MFX_EXTBUFF_VPP_SCENE_ANALYSIS;
	vpp_not_used.AlgList[2] = MFX_EXTBUFF_VPP_DETAIL;
	vpp_not_used.AlgList[3] = MFX_EXTBUFF_VPP_PROCAMP;

	filter->vpp_ext_params = g_list_prepend(filter->vpp_ext_params, &vpp_not_used);

	filter->params.ExtParam = (mfxExtBuffer **) filter->vpp_ext_params->data;
	filter->params.NumExtParam = (mfxU16) g_list_length (filter->vpp_ext_params);
}


static gboolean
gst_mfx_filter_init(GstMfxFilter * filter,
	GstMfxContext * context, mfxSession * session)
{
	mfxFrameAllocResponse response;
	mfxStatus sts;

	memset(&filter->params, 0, sizeof (filter->params));

	filter->params.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY |
        MFX_IOPATTERN_OUT_VIDEO_MEMORY;
	filter->context = gst_mfx_context_ref(context);
	filter->session = *session;

	return TRUE;
}

static void
gst_mfx_filter_finalize(GstMfxFilter * filter)
{
    guint i;

	for (i = 0; i < 2; i++) {
        if (!filter->vpp_request[0])
            continue;
        gst_mfx_surface_pool_unref(filter->vpp_pool[i]);
	}

	//MFXVideoVPP_Close(filter->session);
	gst_mfx_context_unref(filter->context);
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
gst_mfx_filter_new(GstMfxContext * context, mfxSession * session)
{
	GstMfxFilter *filter;

	g_return_val_if_fail(context != NULL, NULL);

	filter = (GstMfxFilter *)
		gst_mfx_mini_object_new0(gst_mfx_filter_class());
	if (!filter)
		return NULL;

	if (!gst_mfx_filter_init(filter, context, session))
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
