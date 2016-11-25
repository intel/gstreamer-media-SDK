/*
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

#ifndef GST_MFXENC_MPEG2_H
#define GST_MFXENC_MPEG2_H

#include <gst/gst.h>
#include "gstmfxenc.h"

G_BEGIN_DECLS

#define GST_TYPE_MFXENC_MPEG2 \
  (gst_mfxenc_mpeg2_get_type ())
#define GST_MFXENC_MPEG2_CAST(obj) \
  ((GstMfxEncMpeg2 *)(obj))
#define GST_MFXENC_MPEG2(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MFXENC_MPEG2, \
  GstMfxEncMpeg2))
#define GST_MFXENC_MPEG2_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MFXENC_MPEG2, \
  GstMfxEncMpeg2Class))
#define GST_MFXENC_MPEG2_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_MFXENC_MPEG2, \
  GstMfxEncMpeg2Class))
#define GST_IS_MFXENC_MPEG2(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MFXENC_MPEG2))
#define GST_IS_MFXENC_MPEG2_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MFXENC_MPEG2))

typedef struct _GstMfxEncMpeg2 GstMfxEncMpeg2;
typedef struct _GstMfxEncMpeg2Class GstMfxEncMpeg2Class;

struct _GstMfxEncMpeg2
{
  /*< private >*/
  GstMfxEnc parent_instance;
};

struct _GstMfxEncMpeg2Class
{
  /*< private >*/
  GstMfxEncClass parent_class;
};

GType
gst_mfxenc_mpeg2_get_type(void);

G_END_DECLS

#endif /* GST_MFXENC_MPEG2_H */
