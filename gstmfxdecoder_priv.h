#ifndef GST_MFX_DECODER_PRIV_H
#define GST_MFX_DECODER_PRIV_H

#include <glib.h>
#include "gstmfxdecoder.h"
#include "gstmfxcontext.h"
#include "gstmfxminiobject.h"

G_BEGIN_DECLS

#define GST_MFX_DECODER_CAST(decoder) \
	((GstMfxDecoder *)(decoder))

#define GST_MFX_DECODER_CLASS(klass) \
	((GstMfxDecoderClass *)(klass))

#define GST_MFX_IS_DECODER_CLASS(klass) \
	((klass) != NULL))

#define GST_MFX_DECODER_GET_CLASS(obj) \
	GST_MFX_DECODER_CLASS(GST_MFX_MINI_OBJECT_GET_CLASS(obj))

typedef struct _GstMfxDecoderClass GstMfxDecoderClass;
struct _GstMfxDecoderUnit;

/**
* GST_MFX_DECODER_DISPLAY:
* @decoder: a #GstMfxDecoder
*
* Macro that evaluates to the #GstMfxDisplay of @decoder.
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_DECODER_DISPLAY
#define GST_MFX_DECODER_DISPLAY(decoder) \
	GST_MFX_DECODER_CAST(decoder)->display

/**
* GST_MFX_DECODER_CONTEXT:
* @decoder: a #GstMfxDecoder
*
* Macro that evaluates to the #GstMfxContext of @decoder.
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_DECODER_CONTEXT
#define GST_MFX_DECODER_CONTEXT(decoder) \
	GST_MFX_DECODER_CAST(decoder)->context

/**
* GST_MFX_DECODER_CODEC:
* @decoder: a #GstMfxDecoder
*
* Macro that evaluates to the #GstMfxCodec of @decoder.
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_DECODER_CODEC
#define GST_MFX_DECODER_CODEC(decoder) \
	GST_MFX_DECODER_CAST(decoder)->codec

/**
* GST_MFX_DECODER_CODEC_STATE:
* @decoder: a #GstMfxDecoder
*
* Macro that evaluates to the #GstVideoCodecState holding codec state
* for @decoder.
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_DECODER_CODEC_STATE
#define GST_MFX_DECODER_CODEC_STATE(decoder) \
	GST_MFX_DECODER_CAST(decoder)->codec_state

/**
* GST_MFX_DECODER_CODEC_DATA:
* @decoder: a #GstMfxDecoder
*
* Macro that evaluates to the #GstBuffer holding optional codec data
* for @decoder.
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_DECODER_CODEC_DATA
#define GST_MFX_DECODER_CODEC_DATA(decoder) \
	GST_MFX_DECODER_CODEC_STATE(decoder)->codec_data

/**
* GST_MFX_DECODER_CODEC_FRAME:
* @decoder: a #GstMfxDecoder
*
* Macro that evaluates to the #GstVideoCodecFrame holding decoder
* units for the current frame.
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_DECODER_CODEC_FRAME
#define GST_MFX_DECODER_CODEC_FRAME(decoder) \
	GST_MFX_PARSER_STATE(decoder)->current_frame

/**
* GST_MFX_DECODER_WIDTH:
* @decoder: a #GstMfxDecoder
*
* Macro that evaluates to the coded width of the picture
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_DECODER_WIDTH
#define GST_MFX_DECODER_WIDTH(decoder) \
	GST_MFX_DECODER_CODEC_STATE(decoder)->info.width

/**
* GST_MFX_DECODER_HEIGHT:
* @decoder: a #GstMfxDecoder
*
* Macro that evaluates to the coded height of the picture
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_DECODER_HEIGHT
#define GST_MFX_DECODER_HEIGHT(decoder) \
	GST_MFX_DECODER_CODEC_STATE(decoder)->info.height

/* End-of-Stream buffer */
#define GST_BUFFER_FLAG_EOS (GST_BUFFER_FLAG_LAST + 0)

#define GST_BUFFER_IS_EOS(buffer) \
	GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_EOS)

#define GST_MFX_DECODER_GET_PRIVATE(obj)                      \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj),                         \
	GST_MFX_TYPE_DECODER,        \
	GstMfxDecoderPrivate))


/**
* GstMfxDecoder:
*
* A VA decoder base instance.
*/
struct _GstMfxDecoder
{
	/*< private >*/
	GstMfxMiniObject parent_instance;

	gpointer user_data;
	GstMfxDisplay *display;
	GstMfxContext *context;
	
	GstVideoCodecState *codec_state;
	GAsyncQueue *buffers;
	GAsyncQueue *frames;

	GstMfxDecoderStateChangedFunc codec_state_changed_func;
	gpointer codec_state_changed_data;

	mfxVideoParam param;
};

/**
* GstMfxDecoderClass:
*
* A VA decoder base class.
*/
struct _GstMfxDecoderClass
{
	/*< private >*/
	GstMfxMiniObjectClass parent_class;

	gboolean(*create) (GstMfxDecoder * decoder);
	void(*destroy) (GstMfxDecoder * decoder);
	GstMfxDecoderStatus(*parse) (GstMfxDecoder * decoder,
		GstAdapter * adapter, gboolean at_eos,
	struct _GstMfxDecoderUnit * unit);
	GstMfxDecoderStatus(*decode) (GstMfxDecoder * decoder,
	struct _GstMfxDecoderUnit * unit);
	GstMfxDecoderStatus(*start_frame) (GstMfxDecoder * decoder,
	struct _GstMfxDecoderUnit * unit);
	GstMfxDecoderStatus(*end_frame) (GstMfxDecoder * decoder);
	GstMfxDecoderStatus(*flush) (GstMfxDecoder * decoder);
	GstMfxDecoderStatus(*decode_codec_data) (GstMfxDecoder * decoder,
		const guchar * buf, guint buf_size);
};


GstMfxDecoder *
gst_mfx_decoder_new(const GstMfxDecoderClass * klass,
	GstMfxDisplay * display, GstCaps * caps);

void
gst_mfx_decoder_finalize(GstMfxDecoder * decoder);

void
gst_mfx_decoder_set_picture_size(GstMfxDecoder * decoder,
guint width, guint height);

G_GNUC_INTERNAL
void
gst_mfx_decoder_set_framerate(GstMfxDecoder * decoder,
guint fps_n, guint fps_d);

G_GNUC_INTERNAL
void
gst_mfx_decoder_set_pixel_aspect_ratio(GstMfxDecoder * decoder,
guint par_n, guint par_d);

G_GNUC_INTERNAL
void
gst_mfx_decoder_set_interlace_mode(GstMfxDecoder * decoder,
GstVideoInterlaceMode mode);

G_GNUC_INTERNAL
void
gst_mfx_decoder_set_interlaced(GstMfxDecoder * decoder,
gboolean interlaced);

gboolean
gst_mfx_decoder_ensure_context(GstMfxDecoder * decoder,
	GstMfxContextInfo * cip);

void
gst_mfx_decoder_push_frame(GstMfxDecoder * decoder,
	GstVideoCodecFrame * frame);

GstMfxDecoderStatus
gst_mfx_decoder_decode_codec_data(GstMfxDecoder * decoder);

G_END_DECLS

#endif /* GST_MFX_DECODER_PRIV_H */