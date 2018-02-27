/*
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

#ifndef GST_MFXENC_H265_H
#define GST_MFXENC_H265_H

#include <gst/gst.h>
#include "gstmfxenc.h"

G_BEGIN_DECLS

#define GST_TYPE_MFXENC_H265 \
    (gst_mfxenc_h265_get_type ())
#define GST_MFXENC_H265_CAST(obj) \
    ((GstMfxEncH265 *)(obj))
#define GST_MFXENC_H265(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MFXENC_H265, \
    GstMfxEncH265))
#define GST_MFXENC_H265_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MFXENC_H265, \
    GstMfxEncH265Class))
#define GST_MFXENC_H265_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_MFXENC_H265, \
    GstMfxEncH265Class))
#define GST_IS_MFXENC_H265(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MFXENC_H265))
#define GST_IS_MFXENC_H265_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MFXENC_H265))

typedef struct _GstMfxEncH265 GstMfxEncH265;
typedef struct _GstMfxEncH265Class GstMfxEncH265Class;

struct _GstMfxEncH265
{
  /*< private > */
  GstMfxEnc parent_instance;

  guint is_hvc:1;               /* [FALSE]=byte-stream (default); [TRUE]=hvcC */
};

struct _GstMfxEncH265Class
{
  /*< private > */
  GstMfxEncClass parent_class;
};

GType
gst_mfxenc_h265_get_type (void);

G_END_DECLS
#endif /* GST_MFXENC_H265_H */
