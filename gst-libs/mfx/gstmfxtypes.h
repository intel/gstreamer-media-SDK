#ifndef GST_MFX_TYPES_H
#define GST_MFX_TYPES_H

#include <glib.h>

G_BEGIN_DECLS

/**
* GstMfxID:
*
* An integer large enough to hold a generic VA id or a pointer
* wherever necessary.
*/
typedef gsize GstMfxID;

/**
* GST_MFX_ID:
* @id: an arbitrary integer value
*
* Macro that creates a #GstMfxID from @id.
*/
#define GST_MFX_ID(id) ((GstMfxID)(id))

/**
* GST_MFX_ID_INVALID:
*
* Macro that evaluates to an invalid #GstMfxID value.
*/
#define GST_MFX_ID_INVALID GST_MFX_ID((gssize)(gint32)-1)

/**
* GST_MFX_ID_FORMAT:
*
* Can be used together with #GST_MFX_ID_ARGS to properly output an
* integer value in a printf()-style text message.
* <informalexample>
* <programlisting>
* printf("id: %" GST_MFX_ID_FORMAT "\n", GST_MFX_ID_ARGS(id));
* </programlisting>
* </informalexample>
*/
#define GST_MFX_ID_FORMAT "p"

/**
* GST_MFX_ID_ARGS:
* @id: a #GstMfxID
*
* Can be used together with #GST_MFX_ID_FORMAT to properly output
* an integer value in a printf()-style text message.
*/
#define GST_MFX_ID_ARGS(id) GSIZE_TO_POINTER(id)

/**
* GstMfxRectangle:
* @x: X coordinate
* @y: Y coordinate
* @width: region width
* @height: region height
*
* A rectangle region within a surface.
*/
typedef struct _GstMfxRectangle GstMfxRectangle;
struct _GstMfxRectangle {
	guint32 x;
	guint32 y;
	guint32 width;
	guint32 height;
};

/**
* GstMfxRotation:
* @GST_MFX_ROTATION_0: the VA display is not rotated.
* @GST_MFX_ROTATION_90: the VA display is rotated by 90°, clockwise.
* @GST_MFX_ROTATION_180: the VA display is rotated by 180°, clockwise.
* @GST_MFX_ROTATION_270: the VA display is rotated by 270°, clockwise.
*/
typedef enum {
	GST_MFX_ROTATION_0 = 0,
	GST_MFX_ROTATION_90 = 90,
	GST_MFX_ROTATION_180 = 180,
	GST_MFX_ROTATION_270 = 270,
} GstMfxRotation;

typedef enum {
	GST_MFX_RATECONTROL_NONE = 0,
	GST_MFX_RATECONTROL_CQP,
	GST_MFX_RATECONTROL_CBR,
	GST_MFX_RATECONTROL_VCM,
	GST_MFX_RATECONTROL_VBR,
	GST_MFX_RATECONTROL_VBR_CONSTRAINED,
} GstMfxRateControl;

/* Define a mask for GstMfxRateControl */
#define GST_MFX_RATECONTROL_MASK(RC) \
	(1 << G_PASTE(GST_MFX_RATECONTROL_,RC))

G_END_DECLS

#endif /* GST_MFX_TYPES_H */
