/*
 *  Copyright (C) 2012-2014 Intel Corporation
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

#ifndef GST_MFX_MINI_OBJECT_H
#define GST_MFX_MINI_OBJECT_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GstMfxMiniObject              GstMfxMiniObject;
typedef struct _GstMfxMiniObjectClass         GstMfxMiniObjectClass;

/**
* GST_MFX_MINI_OBJECT:
* @object: a #GstMfxMiniObject
*
* Casts the @object to a #GstMfxMiniObject
*/
#define GST_MFX_MINI_OBJECT(object) \
	((GstMfxMiniObject *) (object))

/**
* GST_MFX_MINI_OBJECT_PTR:
* @object_ptr: a pointer #GstMfxMiniObject
*
* Casts the @object_ptr to a pointer to #GstMfxMiniObject
*/
#define GST_MFX_MINI_OBJECT_PTR(object_ptr) \
	((GstMfxMiniObject **) (object_ptr))

/**
* GST_MFX_MINI_OBJECT_CLASS:
* @klass: a #GstMfxMiniObjectClass
*
* Casts the @klass to a #GstMfxMiniObjectClass
*/
#define GST_MFX_MINI_OBJECT_CLASS(klass) \
	((GstMfxMiniObjectClass *) (klass))

/**
* GST_MFX_MINI_OBJECT_GET_CLASS:
* @object: a #GstMfxMiniObject
*
* Retrieves the #GstMfxMiniObjectClass associated with the @object
*/
#define GST_MFX_MINI_OBJECT_GET_CLASS(object) \
	(GST_MFX_MINI_OBJECT (object)->object_class)

/**
* GST_MFX_MINI_OBJECT_FLAGS:
* @object: a #GstMfxMiniObject
*
* The entire set of flags for the @object
*/
#define GST_MFX_MINI_OBJECT_FLAGS(object) \
	(GST_MFX_MINI_OBJECT (object)->flags)

/**
* GST_MFX_MINI_OBJECT_FLAG_IS_SET:
* @object: a #GstMfxMiniObject
* @flag: a flag to check for
*
* Checks whether the given @flag is set
*/
#define GST_MFX_MINI_OBJECT_FLAG_IS_SET(object, flag) \
	((GST_MFX_MINI_OBJECT_FLAGS (object) & (flag)) != 0)

/**
* GST_MFX_MINI_OBJECT_FLAG_SET:
* @object: a #GstMfxMiniObject
* @flags: flags to set
*
* This macro sets the given bits
*/
#define GST_MFX_MINI_OBJECT_FLAG_SET(object, flags) \
	(GST_MFX_MINI_OBJECT_FLAGS (object) |= (flags))

/**
* GST_MFX_MINI_OBJECT_FLAG_UNSET:
* @object: a #GstMfxMiniObject
* @flags: flags to unset
*
* This macro unsets the given bits.
*/
#define GST_MFX_MINI_OBJECT_FLAG_UNSET(object, flags) \
	(GST_MFX_MINI_OBJECT_FLAGS (object) &= ~(flags))

/**
* GstMfxMiniObject:
* @object_class: the #GstMfxMiniObjectClass
* @ref_count: the object reference count that should be manipulated
*   through gst_mfx_mini_object_ref () et al. helpers
* @flags: set of flags that should be manipulated through
*   GST_MFX_MINI_OBJECT_FLAG_*() functions
*
* A #GstMfxMiniObject represents a minimal reference counted data
* structure that can hold a set of flags and user-provided data.
*/
struct _GstMfxMiniObject
{
	/*< private >*/
	gconstpointer object_class;
	volatile gint ref_count;
	guint flags;
};

/**
* GstMfxMiniObjectClass:
* @size: size in bytes of the #GstMfxMiniObject, plus any
*   additional data for derived classes
* @finalize: function called to destroy data in derived classes
*
* A #GstMfxMiniObjectClass represents the base object class that
* defines the size of the #GstMfxMiniObject and utility function to
* dispose child objects
*/
struct _GstMfxMiniObjectClass
{
	/*< protected >*/
	guint size;
	GDestroyNotify finalize;
};

GstMfxMiniObject *
gst_mfx_mini_object_new (const GstMfxMiniObjectClass * object_class);

GstMfxMiniObject *
gst_mfx_mini_object_new0 (const GstMfxMiniObjectClass * object_class);

GstMfxMiniObject *
gst_mfx_mini_object_ref (GstMfxMiniObject * object);

void
gst_mfx_mini_object_unref (GstMfxMiniObject * object);

void
gst_mfx_mini_object_replace (GstMfxMiniObject ** old_object_ptr,
GstMfxMiniObject * new_object);

#undef  gst_mfx_mini_object_ref
#define gst_mfx_mini_object_ref(object) \
	gst_mfx_mini_object_ref_internal(object)

#undef  gst_mfx_mini_object_unref
#define gst_mfx_mini_object_unref(object) \
	gst_mfx_mini_object_unref_internal(object)

void
gst_mfx_mini_object_free (GstMfxMiniObject * object);

/**
* gst_mfx_mini_object_ref_internal:
* @object: a #GstMfxMiniObject
*
* Atomically increases the reference count of the given @object by one.
* This is an internal function that does not do any run-time type check.
*
* Returns: The same @object argument
*/
static inline GstMfxMiniObject *
gst_mfx_mini_object_ref_internal (GstMfxMiniObject * object)
{
	g_atomic_int_inc (&object->ref_count);
	return object;
}

/**
* gst_mfx_mini_object_unref_internal:
* @object: a #GstMfxMiniObject
*
* Atomically decreases the reference count of the @object by one. If
* the reference count reaches zero, the object will be free'd.
*
* This is an internal function that does not do any run-time type check.
*/
static inline void
gst_mfx_mini_object_unref_internal (GstMfxMiniObject * object)
{
	if (g_atomic_int_dec_and_test (&object->ref_count))
		gst_mfx_mini_object_free (object);
}

G_END_DECLS

#endif /* GST_MFX_MINI_OBJECT_H */
