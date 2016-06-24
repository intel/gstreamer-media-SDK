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

#include "sysdeps.h"
#include "gstmfxobject.h"
#include "gstmfxobject_priv.h"
#include "gstmfxminiobject.h"
#include "gstmfxdisplay_priv.h"


/* Ensure those symbols are actually defined in the resulting libraries */
#undef gst_mfx_object_ref
#undef gst_mfx_object_unref
#undef gst_mfx_object_replace

static void
gst_mfx_object_finalize (GstMfxObject * object)
{
  const GstMfxObjectClass *const klass = GST_MFX_OBJECT_GET_CLASS (object);

  if (klass->finalize)
    klass->finalize (object);
  gst_mfx_display_replace (&object->display, NULL);
}

void
gst_mfx_object_class_init (GstMfxObjectClass * klass, guint size)
{
  GstMfxMiniObjectClass *const object_class = GST_MFX_MINI_OBJECT_CLASS (klass);

  object_class->size = size;
  object_class->finalize = (GDestroyNotify) gst_mfx_object_finalize;
}

/**
 * gst_mfx_object_new:
 * @klass: The object class
 * @display: The #GstMfxDisplay
 *
 * Creates a new #GstMfxObject. The @klass argument shall not be
 * %NULL, and it must reference a statically allocated descriptor.
 *
 * This function zero-initializes the derived object data. Also note
 * that this is an internal function that shall not be used outside of
 * libgstmfx libraries.
 *
 * Returns: The newly allocated #GstMfxObject
 */
gpointer
gst_mfx_object_new (const GstMfxObjectClass * klass, GstMfxDisplay * display)
{
  const GstMfxMiniObjectClass *const object_class =
      GST_MFX_MINI_OBJECT_CLASS (klass);
  GstMfxObject *object;
  guint sub_size;

  g_return_val_if_fail (klass != NULL, NULL);
  g_return_val_if_fail (display != NULL, NULL);

  object = (GstMfxObject *) gst_mfx_mini_object_new (object_class);
  if (!object)
    return NULL;

  object->display = gst_mfx_display_ref (display);
  object->object_id = VA_INVALID_ID;

  sub_size = object_class->size - sizeof (*object);
  if (sub_size > 0)
    memset (((guchar *) object) + sizeof (*object), 0, sub_size);

  if (klass && klass->init)
    klass->init (object);
  return object;
}

/**
 * gst_mfx_object_ref:
 * @object: a #GstMfxObject
 *
 * Atomically increases the reference count of the given @object by one.
 *
 * Returns: The same @object argument
 */
gpointer
gst_mfx_object_ref (gpointer object)
{
  return gst_mfx_object_ref_internal (object);
}

/**
 * gst_mfx_object_unref:
 * @object: a #GstMfxObject
 *
 * Atomically decreases the reference count of the @object by one. If
 * the reference count reaches zero, the object will be free'd.
 */
void
gst_mfx_object_unref (gpointer object)
{
  gst_mfx_object_unref_internal (object);
}

/**
 * gst_mfx_object_replace:
 * @old_object_ptr: a pointer to a #GstMfxObject
 * @new_object: a #GstMfxObject
 *
 * Atomically replaces the object object held in @old_object_ptr with
 * @new_object. This means that @old_object_ptr shall reference a
 * valid object. However, @new_object can be NULL.
 */
void
gst_mfx_object_replace (gpointer old_object_ptr, gpointer new_object)
{
  gst_mfx_object_replace_internal (old_object_ptr, new_object);
}

/**
 * gst_mfx_object_get_display:
 * @object: a #GstMfxObject
 *
 * Returns the #GstMfxDisplay this @object is bound to.
 *
 * Return value: the parent #GstMfxDisplay object
 */
GstMfxDisplay *
gst_mfx_object_get_display (GstMfxObject * object)
{
  g_return_val_if_fail (object != NULL, NULL);

  return GST_MFX_OBJECT_DISPLAY (object);
}

/**
 * gst_mfx_object_lock_display:
 * @object: a #GstMfxObject
 *
 * Locks @object parent display. If display is already locked by
 * another thread, the current thread will block until display is
 * unlocked by the other thread.
 */
void
gst_mfx_object_lock_display (GstMfxObject * object)
{
  g_return_if_fail (object != NULL);

  GST_MFX_OBJECT_LOCK_DISPLAY (object);
}

/**
 * gst_mfx_object_unlock_display:
 * @object: a #GstMfxObject
 *
 * Unlocks @object parent display. If another thread is blocked in a
 * gst_mfx_object_lock_display() call, it will be woken and can lock
 * display itself.
 */
void
gst_mfx_object_unlock_display (GstMfxObject * object)
{
  g_return_if_fail (object != NULL);

  GST_MFX_OBJECT_UNLOCK_DISPLAY (object);
}

/**
 * gst_mfx_object_get_id:
 * @object: a #GstMfxObject
 *
 * Returns the #GstMfxID contained in the @object.
 *
 * Return value: the #GstMfxID of the @object
 */
GstMfxID
gst_mfx_object_get_id (GstMfxObject * object)
{
  g_return_val_if_fail (object != NULL, 0);

  return GST_MFX_OBJECT_ID (object);
}
