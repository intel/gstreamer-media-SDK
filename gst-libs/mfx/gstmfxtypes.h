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
* @GST_MFX_ROTATION_0: the output surface is not rotated.
* @GST_MFX_ROTATION_90: the output surface is rotated by 90°, clockwise.
* @GST_MFX_ROTATION_180: the output surface is rotated by 180°, clockwise.
* @GST_MFX_ROTATION_270: the output surface is rotated by 270°, clockwise.
*/
typedef enum {
	GST_MFX_ROTATION_0 = 0,
	GST_MFX_ROTATION_90 = 90,
	GST_MFX_ROTATION_180 = 180,
	GST_MFX_ROTATION_270 = 270,
} GstMfxRotation;

G_END_DECLS

#endif /* GST_MFX_TYPES_H */
