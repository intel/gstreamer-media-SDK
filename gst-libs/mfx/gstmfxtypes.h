/*
 *  Copyright (C) 2010-2011 Splitted-Desktop Systems
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@splitted-desktop.com>
 *  Copyright (C) 2012-2014 Intel Corporation
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
 *  Copyright (C) 2016 Intel Corporation
 *    Author: Ishmael Visayana Sameen <ishmael.visayana.sameen@intel.com>
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
#include <mfxvideo.h>

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
struct _GstMfxRectangle
{
  guint32 x;
  guint32 y;
  guint32 width;
  guint32 height;
};

typedef struct _GstMfxMemoryId GstMfxMemoryId;
struct _GstMfxMemoryId
{
  mfxMemId mid;
  mfxMemId mid_stage;
  mfxU16 rw;
  mfxFrameInfo *info;
};

enum
{
  MFX_SURFACE_READ = (1 << 0),
  MFX_SURFACE_WRITE = (1 << 1),
};

typedef enum
{
  GST_MFX_OPTION_AUTO = -1,
  GST_MFX_OPTION_OFF,
  GST_MFX_OPTION_ON,
} GstMfxOption;

typedef enum
{
  GST_MFX_RATECONTROL_NONE = 0,
  GST_MFX_RATECONTROL_CQP = MFX_RATECONTROL_CQP,
  GST_MFX_RATECONTROL_CBR = MFX_RATECONTROL_CBR,
  GST_MFX_RATECONTROL_VCM = MFX_RATECONTROL_VCM,
  GST_MFX_RATECONTROL_VBR = MFX_RATECONTROL_VBR,
  GST_MFX_RATECONTROL_AVBR = MFX_RATECONTROL_AVBR,
  GST_MFX_RATECONTROL_QVBR = MFX_RATECONTROL_QVBR,
  GST_MFX_RATECONTROL_LA_BRC = MFX_RATECONTROL_LA,
  GST_MFX_RATECONTROL_ICQ = MFX_RATECONTROL_ICQ,
  GST_MFX_RATECONTROL_LA_ICQ = MFX_RATECONTROL_LA_ICQ,
  GST_MFX_RATECONTROL_LA_HRD = MFX_RATECONTROL_LA_HRD,
} GstMfxRateControl;

/* Define a mask for GstVaapiRateControl */
#define GST_MFX_RATECONTROL_MASK(RC) \
    (1U << G_PASTE(GST_MFX_RATECONTROL_,RC))

G_END_DECLS
#endif /* GST_MFX_TYPES_H */
