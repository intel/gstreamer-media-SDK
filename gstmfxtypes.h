#ifndef GST_MFX_TYPES_H
#define GST_MFX_TYPES_H

#include <glib.h>

G_BEGIN_DECLS

/**
* GstMfxPoint:
* @x: X coordinate
* @y: Y coordinate
*
* A location within a surface.
*/
typedef struct _GstMfxPoint GstMfxPoint;
struct _GstMfxPoint {
	guint32 x;
	guint32 y;
};

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
* GstMfxRenderMode:
* @GST_MFX_RENDER_MODE_OVERLAY: in this mode, the VA display
*   backend renders surfaces with an overlay engine. This means that
*   the surface that is currently displayed shall not be re-used
*   right away for decoding. i.e. it needs to be retained further,
*   until the next surface is to be displayed.
* @GST_MFX_RENDER_MODE_TEXTURE: in this modem the VA display
*   backend renders surfaces with a textured blit (GPU/3D engine).
*   This means that the surface is copied to some intermediate
*   backing store, or back buffer of a frame buffer, and is free to
*   be re-used right away for decoding.
*/
typedef enum {
	GST_MFX_RENDER_MODE_OVERLAY = 1,
	GST_MFX_RENDER_MODE_TEXTURE
} GstMfxRenderMode;

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

/**
* GstMfxRateControl:
* @GST_MFX_RATECONTROL_NONE: No rate control performed by the
*   underlying driver
* @GST_MFX_RATECONTROL_CQP: Constant QP
* @GST_MFX_RATECONTROL_CBR: Constant bitrate
* @GST_MFX_RATECONTROL_VCM: Video conference mode
* @GST_MFX_RATECONTROL_VBR: Variable bitrate
* @GST_MFX_RATECONTROL_VBR_CONSTRAINED: Variable bitrate with peak
*   rate higher than average bitrate
*
* The set of allowed rate control values for #GstMfxRateControl.
* Note: this is only valid for encoders.
*/
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
