#include <mfxplugin.h>
#include "gstmfxdecoder.h"
#include "gstmfxobjectpool_priv.h"
#include "gstmfxvideometa.h"

#define DEBUG 1
#include "gstmfxdebug.h"

/**
* gst_mfx_decoder_ref:
* @decoder: a #GstMfxDecoder
*
* Atomically increases the reference count of the given @decoder by one.
*
* Returns: The same @decoder argument
*/
GstMfxDecoder *
gst_mfx_decoder_ref(GstMfxDecoder * decoder)
{
	return gst_mfx_mini_object_ref(decoder);
}

/**
* gst_mfx_decoder_unref:
* @decoder: a #GstMfxDecoder
*
* Atomically decreases the reference count of the @decoder by one. If
* the reference count reaches zero, the decoder will be free'd.
*/
void
gst_mfx_decoder_unref(GstMfxDecoder * decoder)
{
	gst_mfx_mini_object_unref(decoder);
}

/**
* gst_mfx_decoder_replace:
* @old_decoder_ptr: a pointer to a #GstMfxDecoder
* @new_decoder: a #GstMfxDecoder
*
* Atomically replaces the decoder decoder held in @old_decoder_ptr
* with @new_decoder. This means that @old_decoder_ptr shall reference
* a valid decoder. However, @new_decoder can be NULL.
*/
void
gst_mfx_decoder_replace(GstMfxDecoder ** old_decoder_ptr,
GstMfxDecoder * new_decoder)
{
	gst_mfx_mini_object_replace(old_decoder_ptr, new_decoder);
}

mfxU32
gst_mfx_decoder_get_codec(GstMfxDecoder * decoder)
{
	g_return_val_if_fail(decoder != NULL, -1);

	return decoder->codec;
}

GstMfxDecoderStatus
gst_mfx_decoder_get_surface_proxy(GstMfxDecoder * decoder,
	GstMfxSurfaceProxy ** out_proxy_ptr)
{
	g_return_val_if_fail(decoder != NULL,
		GST_MFX_DECODER_STATUS_ERROR_INVALID_PARAMETER);
	g_return_val_if_fail(out_proxy_ptr != NULL,
		GST_MFX_DECODER_STATUS_ERROR_INVALID_PARAMETER);

	*out_proxy_ptr = g_async_queue_try_pop(decoder->surfaces);

	return GST_MFX_DECODER_STATUS_SUCCESS;
}

static void
gst_mfx_decoder_push_surface(GstMfxDecoder * decoder, GstMfxSurfaceProxy * proxy)
{
	g_async_queue_push(decoder->surfaces, proxy);
}

static void
gst_mfx_decoder_finalize(GstMfxDecoder *decoder)
{
	gst_mfx_object_pool_unref(decoder->pool);

	g_slice_free1(decoder->bs.MaxLength, decoder->bs.Data);

	MFXVideoDECODE_Close(decoder->session);
}


static void
gst_mfx_decoder_init(GstMfxDecoder * decoder,
	GstMfxDisplay * display, mfxU32 codec_id,
	GstMfxContextAllocatorVaapi * ctx)
{
	memset(&(decoder->session), 0, sizeof (mfxSession));
	memset(&(decoder->bs), 0, sizeof (mfxBitstream));
	memset(&(decoder->param), 0, sizeof (mfxVideoParam));

	decoder->codec = codec_id;
	decoder->param.mfx.CodecId = codec_id;
	decoder->param.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
	decoder->alloc_ctx = ctx;
	decoder->display = gst_mfx_display_ref(display);
	decoder->context = NULL;
	decoder->surfaces = g_async_queue_new();
	decoder->pool = NULL;
	decoder->decoder_inited = FALSE;
}

static inline const GstMfxMiniObjectClass *
gst_mfx_decoder_class(void)
{
	static const GstMfxMiniObjectClass GstMfxDecoderClass = {
		sizeof(GstMfxDecoder),
		(GDestroyNotify)gst_mfx_decoder_finalize
	};
	return &GstMfxDecoderClass;
}

GstMfxDecoder *
gst_mfx_decoder_new(GstMfxDisplay * display,
	GstMfxContextAllocatorVaapi *allocator, mfxU32 codec_id)
{
	GstMfxDecoder *decoder;

	decoder = gst_mfx_mini_object_new(gst_mfx_decoder_class());
	if (!decoder)
		goto error;

	gst_mfx_decoder_init(decoder, display, codec_id, allocator);

	return decoder;

error:
	gst_mfx_mini_object_unref(decoder);
	return NULL;
}

static gboolean
gst_mfx_decoder_ensure_context(GstMfxDecoder *decoder)
{
	if (!decoder->context) {
		decoder->context = gst_mfx_context_new(decoder->alloc_ctx);
		if (!decoder->context)
			return FALSE;

		decoder->session = gst_mfx_context_get_session(decoder->context);
		MFXVideoCORE_SetHandle(decoder->session, MFX_HANDLE_VA_DISPLAY, decoder->alloc_ctx->va_dpy);
	}

	return TRUE;
}

static mfxStatus
gst_mfx_decoder_load_decoder_plugins(GstMfxDecoder *decoder)
{
    mfxPluginUID uid;
    mfxStatus sts;

    switch (decoder->codec) {
    case MFX_CODEC_HEVC:
    {
        gchar *plugin_uids[] = { "33a61c0b4c27454ca8d85dde757c6f8e",
                                 "15dd936825ad475ea34e35f3f54217a6",
                                 NULL };
        guint i, c;
        for (i = 0; plugin_uids[i]; i++) {
            for (c = 0; c < sizeof(uid.Data); c++)
                sscanf(plugin_uids[i] + 2 * c, "%2hhx", uid.Data + c);
            sts = MFXVideoUSER_Load(decoder->session, &uid, 1);
            if (MFX_ERR_NONE == sts)
                break;
        }
    }
        break;
    default:
        sts = MFX_ERR_NONE;
    }

    return sts;
}

static gint
sync_output_surface(gconstpointer surface, gconstpointer surf)
{
    GstMfxSurface *_surface = (GstMfxSurface *)surface;
    mfxFrameSurface1 *_surf = (mfxFrameSurface1 *)surf;

    return (*(int *)(_surf->Data.MemId) !=
            *(int *)_surface->surface->Data.MemId);
}

static void
put_unused_frames(gpointer surface, gpointer pool)
{
	GstMfxSurface *_surface = (GstMfxSurface *)surface;
	GstMfxObjectPool *_pool = (GstMfxObjectPool *)pool;

	mfxFrameSurface1 *surf = gst_mfx_surface_get_frame_surface(_surface);
	if (surf && !surf->Data.Locked)
		gst_mfx_object_pool_put_object(_pool, _surface);
}

static gboolean
get_surface(GstMfxDecoder *decoder, GstMfxSurface **surface)
{
	g_list_foreach(decoder->pool->used_objects, put_unused_frames, decoder->pool);

	*surface = gst_mfx_object_pool_get_object(decoder->pool);
    if (!*surface)
		return FALSE;

	return TRUE;
}

static GstMfxDecoderStatus
gst_mfx_decoder_start(GstMfxDecoder *decoder)
{
	GstMfxDecoderStatus ret = GST_MFX_DECODER_STATUS_READY;
	mfxStatus sts = MFX_ERR_NONE;

	if (!gst_mfx_decoder_ensure_context(decoder))
		return GST_MFX_DECODER_STATUS_ERROR_INIT_FAILED;

    sts = gst_mfx_decoder_load_decoder_plugins(decoder);
    if (sts < 0)
        return GST_MFX_DECODER_STATUS_ERROR_UNSUPPORTED_CODEC;

	sts = MFXVideoDECODE_DecodeHeader(decoder->session, &decoder->bs, &decoder->param);
	if (MFX_ERR_MORE_DATA == sts) {
		return GST_MFX_DECODER_STATUS_ERROR_NO_DATA;
	}
	else if (sts < 0) {
		GST_ERROR_OBJECT(decoder, "Decode header error %d\n", sts);
		return GST_MFX_DECODER_STATUS_ERROR_BITSTREAM_PARSER;
	}

	sts = MFXVideoDECODE_Init(decoder->session, &decoder->param);
	if (sts < 0) {
		GST_ERROR_OBJECT(decoder, "Error initializing the MFX video decoder %d", sts);
		return GST_MFX_DECODER_STATUS_ERROR_INIT_FAILED;
	}

	decoder->pool = gst_mfx_surface_pool_new(decoder->display, decoder->alloc_ctx);
	if (!decoder->pool)
		return GST_MFX_DECODER_STATUS_ERROR_ALLOCATION_FAILED;

	return ret;
}

GstMfxDecoderStatus
gst_mfx_decoder_decode(GstMfxDecoder * decoder,
	GstVideoCodecFrame * frame)
{
	GstMapInfo minfo;
	GstMfxDecoderStatus ret = GST_MFX_DECODER_STATUS_SUCCESS;

	GstMfxSurfaceProxy *proxy;
	GstMfxSurface *surface;
	GstMfxRectangle crop_rect;
	mfxFrameSurface1 *insurf;
	mfxFrameSurface1 *outsurf = NULL;
	mfxSyncPoint sync;
	mfxStatus sts = MFX_ERR_NONE;

	if (!gst_buffer_map(frame->input_buffer, &minfo, GST_MAP_READ)) {
		GST_ERROR_OBJECT(decoder, "Failed to map input buffer");
		return GST_MFX_DECODER_STATUS_ERROR_UNKNOWN;
	}

	if (decoder->bs.Data == NULL) {
		decoder->bs.MaxLength = 8192 * 4096;
		decoder->bs.Data = g_slice_alloc(decoder->bs.MaxLength);
	}

	if (minfo.size) {
		memcpy(decoder->bs.Data + decoder->bs.DataOffset + decoder->bs.DataLength, minfo.data, minfo.size);
		decoder->bs.DataLength += minfo.size;
		decoder->bs.TimeStamp = GST_BUFFER_PTS(frame->input_buffer);
	}

	/* Initialize the MFX decoder session */
	if (G_UNLIKELY(!decoder->decoder_inited)) {
		ret = gst_mfx_decoder_start(decoder);
		if (GST_MFX_DECODER_STATUS_READY == ret)
			decoder->decoder_inited = TRUE;

        return ret;
	}

	do {
		if (!get_surface(decoder, &surface))
			return GST_MFX_DECODER_STATUS_ERROR_ALLOCATION_FAILED;

		insurf = gst_mfx_surface_get_frame_surface(surface);
		sts = MFXVideoDECODE_DecodeFrameAsync(decoder->session, &decoder->bs,
			insurf, &outsurf, &sync);

		if (sts == MFX_WRN_DEVICE_BUSY)
			g_usleep(500);
	} while (sts == MFX_WRN_DEVICE_BUSY || sts == MFX_ERR_MORE_SURFACE);

	if (sts == MFX_ERR_MORE_DATA || sts > 0) {
		ret = GST_MFX_DECODER_STATUS_ERROR_NO_DATA;
	}

	if (sts != MFX_ERR_NONE &&
		sts != MFX_ERR_MORE_DATA &&
		sts != MFX_WRN_VIDEO_PARAM_CHANGED) {
		GST_ERROR_OBJECT(decoder, "Error during MFX decoding.");
		ret = GST_MFX_DECODER_STATUS_ERROR_UNKNOWN;
	}

	if (sync) {
		MFXVideoCORE_SyncOperation(decoder->session, sync, 60000);

		memmove(decoder->bs.Data, decoder->bs.Data + decoder->bs.DataOffset,
          decoder->bs.DataLength);
		decoder->bs.DataOffset = 0;

		if (GST_MFX_OBJECT_ID(surface) != *(int*)(outsurf->Data.MemId)) {
            GList *l = g_list_find_custom(decoder->pool->used_objects, outsurf,
                                          sync_output_surface);
            surface = GST_MFX_SURFACE(l->data);
		}

		crop_rect.x = surface->surface->Info.CropX;
		crop_rect.y = surface->surface->Info.CropY;
		crop_rect.width = surface->surface->Info.CropW;
		crop_rect.height = surface->surface->Info.CropH;

        proxy = gst_mfx_surface_proxy_new(surface);
		gst_mfx_surface_proxy_set_crop_rect(proxy, &crop_rect);

		gst_mfx_decoder_push_surface(decoder, proxy);
	}

	gst_buffer_unmap(frame->input_buffer, &minfo);

	return ret;

}
