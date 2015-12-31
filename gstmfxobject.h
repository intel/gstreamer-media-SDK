#ifndef GST_MFX_OBJECT_H
#define GST_MFX_OBJECT_H

#include "gstmfxtypes.h"
#include "gstmfxdisplay.h"

G_BEGIN_DECLS

#define GST_MFX_OBJECT(obj) \
	((GstMfxObject *) (obj))

typedef struct _GstMfxObject GstMfxObject;

/**
* GST_MFX_OBJECT_DISPLAY:
* @object: a #GstMfxObject
*
* Macro that evaluates to the #GstMfxDisplay the @object is bound to.
*/
#define GST_MFX_OBJECT_DISPLAY(object) \
	gst_mfx_object_get_display (GST_MFX_OBJECT (object))

/**
* GST_MFX_OBJECT_ID:
* @object: a #GstMfxObject
*
* Macro that evaluates to the #GstMfxID contained in @object.
*/
#define GST_MFX_OBJECT_ID(object) \
	gst_mfx_object_get_id (GST_MFX_OBJECT (object))

gpointer
gst_mfx_object_ref(gpointer object);

void
gst_mfx_object_unref(gpointer object);

void
gst_mfx_object_replace(gpointer old_object_ptr, gpointer new_object);

GstMfxDisplay *
gst_mfx_object_get_display(GstMfxObject * object);

void
gst_mfx_object_lock_display(GstMfxObject * object);

void
gst_mfx_object_unlock_display(GstMfxObject * object);

GstMfxID
gst_mfx_object_get_id(GstMfxObject * object);

G_END_DECLS

#endif /* GST_MFX_OBJECT_H */