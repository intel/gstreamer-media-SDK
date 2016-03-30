#ifndef GST_MFXPOSTPROC_H
#define GST_MFXPOSTPROC_H

#include "gstmfxpluginbase.h"
#include "gstmfxsurfaceproxy.h"
#include "gstmfxsurfacepool.h"
#include "gstmfxfilter.h"

G_BEGIN_DECLS

#define GST_TYPE_MFXPOSTPROC \
	(gst_mfxpostproc_get_type ())
#define GST_MFXPOSTPROC(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MFXPOSTPROC, GstMfxPostproc))
#define GST_MFXPOSTPROC_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MFXPOSTPROC, \
	GstMfxPostprocClass))
#define GST_IS_MFXPOSTPROC(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_MFXPOSTPROC))
#define GST_IS_MFXPOSTPROC_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_MFXPOSTPROC))
#define GST_MFXPOSTPROC_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_MFXPOSTPROC, \
	GstMfxPostprocClass))

typedef struct _GstMfxPostproc GstMfxPostproc;
typedef struct _GstMfxPostprocClass GstMfxPostprocClass;


/**
* GstMfxPostprocFlags:
* @GST_MFX_POSTPROC_FLAG_FORMAT: Pixel format conversion.
* @GST_MFX_POSTPROC_FLAG_DENOISE: Noise reduction.
* @GST_MFX_POSTPROC_FLAG_DETAIL: Sharpening.
* @GST_MFX_POSTPROC_FLAG_HUE: Change color hue.
* @GST_MFX_POSTPROC_FLAG_SATURATION: Change saturation.
* @GST_MFX_POSTPROC_FLAG_BRIGHTNESS: Change brightness.
* @GST_MFX_POSTPROC_FLAG_CONTRAST: Change contrast.
* @GST_MFX_POSTPROC_FLAG_DEINTERLACE: Deinterlacing.
* @GST_MFX_POSTPROC_FLAG_SIZE: Video scaling.
*
* The set of operations that are to be performed for each frame.
*/
typedef enum
{
	GST_MFX_POSTPROC_FLAG_FORMAT = 1 << GST_MFX_FILTER_OP_FORMAT,
	GST_MFX_POSTPROC_FLAG_DENOISE = 1 << GST_MFX_FILTER_OP_DENOISE,
	GST_MFX_POSTPROC_FLAG_DETAIL = 1 << GST_MFX_FILTER_OP_DETAIL,
	GST_MFX_POSTPROC_FLAG_HUE = 1 << GST_MFX_FILTER_OP_HUE,
	GST_MFX_POSTPROC_FLAG_SATURATION = 1 << GST_MFX_FILTER_OP_SATURATION,
	GST_MFX_POSTPROC_FLAG_BRIGHTNESS = 1 << GST_MFX_FILTER_OP_BRIGHTNESS,
	GST_MFX_POSTPROC_FLAG_CONTRAST = 1 << GST_MFX_FILTER_OP_CONTRAST,
	GST_MFX_POSTPROC_FLAG_DEINTERLACING = 1 << GST_MFX_FILTER_OP_DEINTERLACING,

	/* Additional custom flags */
	GST_MFX_POSTPROC_FLAG_CUSTOM = 1 << 20,
	GST_MFX_POSTPROC_FLAG_SIZE = GST_MFX_POSTPROC_FLAG_CUSTOM,
} GstMfxPostprocFlags;



struct _GstMfxPostproc
{
	/*< private >*/
	GstMfxPluginBase parent_instance;

	GstMfxFilter *filter;
	GstVideoFormat format;        /* output video format */
	guint width;
	guint height;
	guint flags;

	GstMfxTask *task;

	GstCaps *allowed_sinkpad_caps;
	GstVideoInfo sinkpad_info;
	GstCaps *allowed_srcpad_caps;
	GstVideoInfo srcpad_info;

	/* Deinterlacing */
	GstMfxDeinterlaceMode deinterlace_mode;

	/* Basic filter values */
	guint denoise_level;
	guint detail_level;

	/* Color balance filter values */
	gfloat hue;
	gfloat saturation;
	gfloat brightness;
	gfloat contrast;

	guint get_va_surfaces : 1;
	guint use_vpp : 1;
	guint keep_aspect : 1;
};

struct _GstMfxPostprocClass
{
	/*< private >*/
	GstMfxPluginBaseClass parent_class;
};

GType
gst_mfxpostproc_get_type(void);

G_END_DECLS

#endif /* GST_MFXPOSTPROC_H */