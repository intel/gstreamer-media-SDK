/*
 *  Copyright (C) 2010-2011 Splitted-Desktop Systems
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@splitted-desktop.com>
 *  Copyright (C) 2012-2013 Intel Corporation
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#ifndef GST_MFX_OBJECT_PRIV_H
#define GST_MFX_OBJECT_PRIV_H

#include "gstmfxobject.h"
#include "gstmfxminiobject.h"
#include "gstmfxdisplay_priv.h"

G_BEGIN_DECLS

#define GST_MFX_OBJECT_CLASS(klass) \
	((GstMfxObjectClass *) (klass))
#define GST_MFX_IS_OBJECT_CLASS(klass) \
	((klass) != NULL)
#define GST_MFX_OBJECT_GET_CLASS(object) \
	GST_MFX_OBJECT_CLASS (GST_MFX_MINI_OBJECT_GET_CLASS (object))

typedef struct _GstMfxObjectClass GstMfxObjectClass;
typedef void(*GstMfxObjectInitFunc) (GstMfxObject * object);
typedef void(*GstMfxObjectFinalizeFunc) (GstMfxObject * object);

#define GST_MFX_OBJECT_DEFINE_CLASS_WITH_CODE(TN, t_n, code)  \
	static inline const GstMfxObjectClass *                       \
	G_PASTE(t_n,_class) (void)                                      \
{                                                               \
	static G_PASTE(TN,Class) g_class;                           \
	static gsize g_class_init = FALSE;                          \
	\
	if (g_once_init_enter (&g_class_init)) {                    \
	GstMfxObjectClass * const klass =                     \
	GST_MFX_OBJECT_CLASS (&g_class);                  \
	gst_mfx_object_class_init (klass, sizeof(TN));        \
	code;                                                   \
	klass->finalize = (GstMfxObjectFinalizeFunc)          \
	G_PASTE(t_n,_finalize);                             \
	g_once_init_leave (&g_class_init, TRUE);                \
	}                                                           \
	return GST_MFX_OBJECT_CLASS (&g_class);                   \
}

#define GST_MFX_OBJECT_DEFINE_CLASS(TN, t_n) \
	GST_MFX_OBJECT_DEFINE_CLASS_WITH_CODE (TN, t_n, /**/)

/**
* GST_MFX_OBJECT_ID:
* @object: a #GstMfxObject
*
* Macro that evaluates to the #GstMfxID contained in @object.
* This is an internal macro that does not do any run-time type checks.
*/
#undef  GST_MFX_OBJECT_ID
#define GST_MFX_OBJECT_ID(object) \
	(GST_MFX_OBJECT (object)->object_id)

/**
* GST_MFX_OBJECT_DISPLAY:
* @object: a #GstMfxObject
*
* Macro that evaluates to the #GstMfxDisplay the @object is bound to.
* This is an internal macro that does not do any run-time type check.
*/
#undef  GST_MFX_OBJECT_DISPLAY
#define GST_MFX_OBJECT_DISPLAY(object) \
	(GST_MFX_OBJECT (object)->display)

/**
* GST_MFX_OBJECT_DISPLAY_X11:
* @object: a #GstMfxObject
*
* Macro that evaluates to the #GstMfxDisplayX11 the @object is bound to.
* This is an internal macro that does not do any run-time type check
* and requires #include "gstmfxdisplay_x11_priv.h"
*/
#define GST_MFX_OBJECT_DISPLAY_X11(object) \
	GST_MFX_DISPLAY_X11_CAST (GST_MFX_OBJECT_DISPLAY (object))

/**
* GST_MFX_OBJECT_DISPLAY_GLX:
* @object: a #GstMfxObject
*
* Macro that evaluates to the #GstMfxDisplayGLX the @object is bound to.
* This is an internal macro that does not do any run-time type check
* and requires #include "gstmfxdisplay_glx_priv.h".
*/
#define GST_MFX_OBJECT_DISPLAY_GLX(object) \
	GST_MFX_DISPLAY_GLX_CAST (GST_MFX_OBJECT_DISPLAY (object))

/**
* GST_MFX_OBJECT_DISPLAY_WAYLAND:
* @object: a #GstMfxObject
*
* Macro that evaluates to the #GstMfxDisplayWayland the @object is
* bound to.  This is an internal macro that does not do any run-time
* type check and requires #include "gstmfxdisplay_wayland_priv.h"
*/
#define GST_MFX_OBJECT_DISPLAY_WAYLAND(object) \
	GST_MFX_DISPLAY_WAYLAND_CAST (GST_MFX_OBJECT_DISPLAY (object))

/**
* GST_MFX_OBJECT_VADISPLAY:
* @object: a #GstMfxObject
*
* Macro that evaluates to the #VADisplay of @display.
* This is an internal macro that does not do any run-time type check
* and requires #include "gstmfxdisplay_priv.h".
*/
#define GST_MFX_OBJECT_VADISPLAY(object) \
	GST_MFX_DISPLAY_VADISPLAY (GST_MFX_OBJECT_DISPLAY (object))

/**
* GST_MFX_OBJECT_NATIVE_DISPLAY:
* @object: a #GstMfxObject
*
* Macro that evaluates to the underlying native @display object.
* This is an internal macro that does not do any run-time type check.
*/
#define GST_MFX_OBJECT_NATIVE_DISPLAY(object) \
	GST_MFX_DISPLAY_NATIVE (GST_MFX_OBJECT_DISPLAY (object))

/**
* GST_MFX_OBJECT_LOCK_DISPLAY:
* @object: a #GstMfxObject
*
* Macro that locks the #GstMfxDisplay contained in the @object.
* This is an internal macro that does not do any run-time type check.
*/
#define GST_MFX_OBJECT_LOCK_DISPLAY(object) \
	GST_MFX_DISPLAY_LOCK (GST_MFX_OBJECT_DISPLAY (object))

/**
* GST_MFX_OBJECT_UNLOCK_DISPLAY:
* @object: a #GstMfxObject
*
* Macro that unlocks the #GstMfxDisplay contained in the @object.
* This is an internal macro that does not do any run-time type check.
*/
#define GST_MFX_OBJECT_UNLOCK_DISPLAY(object) \
	GST_MFX_DISPLAY_UNLOCK (GST_MFX_OBJECT_DISPLAY (object))

/**
* GstMfxObject:
*
* VA object base.
*/
struct _GstMfxObject
{
	/*< private >*/
	GstMfxMiniObject parent_instance;

	GstMfxDisplay *display;
	GstMfxID object_id;
};

/**
* GstMfxObjectClass:
*
* VA object base class.
*/
struct _GstMfxObjectClass
{
	/*< private >*/
	GstMfxMiniObjectClass parent_class;

	GstMfxObjectInitFunc init;
	GstMfxObjectFinalizeFunc finalize;
};

void
gst_mfx_object_class_init(GstMfxObjectClass * klass, guint size);

gpointer
gst_mfx_object_new(const GstMfxObjectClass * klass,
	GstMfxDisplay * display);

/* Inline reference counting for core libgstmfx library */
static inline gpointer
gst_mfx_object_ref_internal(gpointer object)
{
	return gst_mfx_mini_object_ref(object);
}

static inline void
gst_mfx_object_unref_internal(gpointer object)
{
	gst_mfx_mini_object_unref(object);
}

static inline void
gst_mfx_object_replace_internal(gpointer old_object_ptr, gpointer new_object)
{
	gst_mfx_mini_object_replace((GstMfxMiniObject **)old_object_ptr,
		new_object);
}

#undef  gst_mfx_object_ref
#define gst_mfx_object_ref(object) \
	gst_mfx_object_ref_internal ((object))

#undef  gst_mfx_object_unref
#define gst_mfx_object_unref(object) \
	gst_mfx_object_unref_internal ((object))

#undef  gst_mfx_object_replace
#define gst_mfx_object_replace(old_object_ptr, new_object) \
	gst_mfx_object_replace_internal ((old_object_ptr), (new_object))


G_END_DECLS

#endif /* GST_MFX_OBJECT_PRIV_H */
