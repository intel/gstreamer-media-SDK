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
