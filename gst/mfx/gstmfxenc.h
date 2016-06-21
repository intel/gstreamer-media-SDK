#ifndef GST_MFXENCODE_H
#define GST_MFXENCODE_H

#include "gstmfxpluginbase.h"
#include "gstmfxencoder.h"

G_BEGIN_DECLS

#define GST_TYPE_MFXENC \
	(gst_mfxenc_get_type ())
#define GST_MFXENC_CAST(obj) \
	((GstMfxEnc *)(obj))
#define GST_MFXENC(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MFXENC, GstMfxEnc))
#define GST_MFXENC_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MFXENC, GstMfxEncClass))
#define GST_MFXENC_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_MFXENC, GstMfxEncClass))
#define GST_IS_MFXENC(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MFXENC))
#define GST_IS_MFXENC_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MFXENC))

typedef struct _GstMfxEnc GstMfxEnc;
typedef struct _GstMfxEncClass GstMfxEncClass;

struct _GstMfxEnc
{
	/*< private >*/
	GstMfxPluginBase parent_instance;

	GstMfxEncoder *encoder;
	GstVideoCodecState *input_state;
	gboolean input_state_changed;
	/* needs to be set by the subclass implementation */
	gboolean need_codec_data;
	GstVideoCodecState *output_state;
	GPtrArray *prop_values;
};

struct _GstMfxEncClass
{
	/*< private >*/
	GstMfxPluginBaseClass parent_class;

	GPtrArray *			(*get_properties) (void);
	gboolean			(*get_property)   (GstMfxEnc * encode,
										   guint prop_id, GValue * value);
	gboolean			(*set_property)   (GstMfxEnc * encode,
										   guint prop_id, const GValue * value);

	gboolean			(*set_config)     (GstMfxEnc * encode);
	GstCaps *			(*get_caps)       (GstMfxEnc * encode);
	GstMfxEncoder *		(*alloc_encoder)  (GstMfxEnc * encode);
	gboolean			(*format_buffer)  (GstMfxEnc * encode, GstBuffer ** out_buffer_ptr);
};

GType
gst_mfxenc_get_type(void);

gboolean
gst_mfxenc_init_properties(GstMfxEnc * encode);

gboolean
gst_mfxenc_class_init_properties(GstMfxEncClass * encode_class);

G_END_DECLS

#endif /* GST_MFXENC_H */