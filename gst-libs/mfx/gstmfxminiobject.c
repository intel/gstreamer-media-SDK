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

#include <string.h>
#include "gstmfxminiobject.h"

/* Ensure those symbols are actually defined in the resulting libraries */
#undef gst_mfx_mini_object_ref
#undef gst_mfx_mini_object_unref
#undef gst_mfx_mini_object_replace

void
gst_mfx_mini_object_free(GstMfxMiniObject * object)
{
	const GstMfxMiniObjectClass *const klass = object->object_class;

	g_atomic_int_inc(&object->ref_count);

	if (klass->finalize)
		klass->finalize(object);

	if (G_LIKELY(g_atomic_int_dec_and_test(&object->ref_count)))
		g_slice_free1(klass->size, object);
}

/**
* gst_mfx_mini_object_new:
* @object_class: (optional): The object class
*
* Creates a new #GstMfxMiniObject. If @object_class is NULL, then the
* size of the allocated object is the same as sizeof(GstMfxMiniObject).
* If @object_class is not NULL, typically when a sub-class is implemented,
* that pointer shall reference a statically allocated descriptor.
*
* This function does *not* zero-initialize the derived object data,
* use gst_mfx_mini_object_new0() to fill this purpose.
*
* Returns: The newly allocated #GstMfxMiniObject
*/
GstMfxMiniObject *
gst_mfx_mini_object_new(const GstMfxMiniObjectClass * object_class)
{
	GstMfxMiniObject *object;

	static const GstMfxMiniObjectClass default_object_class = {
		.size = sizeof (GstMfxMiniObject),
	};

	if (G_UNLIKELY(!object_class))
		object_class = &default_object_class;

	g_return_val_if_fail(object_class->size >= sizeof (*object), NULL);

	object = g_slice_alloc(object_class->size);
	if (!object)
		return NULL;

	object->object_class = object_class;
	object->ref_count = 1;
	object->flags = 0;
	return object;
}

/**
* gst_mfx_mini_object_new0:
* @object_class: (optional): The object class
*
* Creates a new #GstMfxMiniObject. This function is similar to
* gst_mfx_mini_object_new() but derived object data is initialized
* to zeroes.
*
* Returns: The newly allocated #GstMfxMiniObject
*/
GstMfxMiniObject *
gst_mfx_mini_object_new0(const GstMfxMiniObjectClass * object_class)
{
	GstMfxMiniObject *object;
	guint sub_size;

	object = gst_mfx_mini_object_new(object_class);
	if (!object)
		return NULL;

	object_class = object->object_class;

	sub_size = object_class->size - sizeof (*object);
	if (sub_size > 0)
		memset(((guchar *)object) + sizeof (*object), 0, sub_size);
	return object;
}

/**
* gst_mfx_mini_object_ref:
* @object: a #GstMfxMiniObject
*
* Atomically increases the reference count of the given @object by one.
*
* Returns: The same @object argument
*/
GstMfxMiniObject *
gst_mfx_mini_object_ref(GstMfxMiniObject * object)
{
	g_return_val_if_fail(object != NULL, NULL);

	return gst_mfx_mini_object_ref_internal(object);
}

/**
* gst_mfx_mini_object_unref:
* @object: a #GstMfxMiniObject
*
* Atomically decreases the reference count of the @object by one. If
* the reference count reaches zero, the object will be free'd.
*/
void
gst_mfx_mini_object_unref(GstMfxMiniObject * object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(object->ref_count > 0);

	gst_mfx_mini_object_unref_internal(object);
}

/**
* gst_mfx_mini_object_replace:
* @old_object_ptr: a pointer to a #GstMfxMiniObject
* @new_object: a #GstMfxMiniObject
*
* Atomically replaces the object held in @old_object_ptr with
* @new_object. This means that @old_object_ptr shall reference a
* valid object. However, @new_object can be NULL.
*/
void
gst_mfx_mini_object_replace(GstMfxMiniObject ** old_object_ptr,
GstMfxMiniObject * new_object)
{
	GstMfxMiniObject *old_object;

	g_return_if_fail(old_object_ptr != NULL);

	old_object = g_atomic_pointer_get((gpointer *)old_object_ptr);

	if (old_object == new_object)
		return;

	if (new_object)
		gst_mfx_mini_object_ref_internal(new_object);

	while (!g_atomic_pointer_compare_and_exchange((gpointer *)old_object_ptr,
		old_object, new_object))
		old_object = g_atomic_pointer_get((gpointer *)old_object_ptr);

	if (old_object)
		gst_mfx_mini_object_unref_internal(old_object);
}
