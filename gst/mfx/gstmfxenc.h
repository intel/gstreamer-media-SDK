/*
 *  Copyright (C) 2013-2014 Intel Corporation
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

#ifndef GST_MFXENCODE_H
#define GST_MFXENCODE_H

#include "gstmfxpluginbase.h"
#include <gst-libs/mfx/gstmfxencoder.h>

G_BEGIN_DECLS

#define GST_TYPE_MFXENC \
    (gst_mfxenc_get_type ())
#define GST_MFXENC_CAST(obj) \
    ((GstMfxEnc *)(obj))
#define GST_MFXENC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MFXENC, GstMfxEnc))
#define GST_MFXENC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MFXENC, GstMfxEncClass))
#define GST_MFXENC_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_MFXENC, GstMfxEncClass))
#define GST_IS_MFXENC(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MFXENC))
#define GST_IS_MFXENC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MFXENC))

typedef struct _GstMfxEnc GstMfxEnc;
typedef struct _GstMfxEncClass GstMfxEncClass;

struct _GstMfxEnc
{
  /*< private > */
  GstMfxPluginBase parent_instance;

  GstMfxEncoder *encoder;
  GstVideoCodecState *input_state;
  gboolean input_state_changed;

  /* needs to be set by the subclass implementation */
  gboolean need_codec_data;
  GstVideoCodecState *output_state;
  GPtrArray *prop_values;
};

struct _GstMfxEncClass
{
  /*< private > */
  GstMfxPluginBaseClass parent_class;

  GPtrArray *(*get_properties) (void);
  gboolean (*get_property) (GstMfxEnc * encode, guint prop_id,
      GValue * value);
  gboolean (*set_property) (GstMfxEnc * encode, guint prop_id,
      const GValue * value);

  gboolean (*set_config) (GstMfxEnc * encode);
  GstCaps *(*get_caps) (GstMfxEnc * encode);
  GstMfxEncoder *(*alloc_encoder) (GstMfxEnc * encode);
  GstFlowReturn (*format_buffer) (GstMfxEnc * encode, GstBuffer * in_buffer,
      GstBuffer ** out_buffer_ptr);
};

GType
gst_mfxenc_get_type (void);

gboolean
gst_mfxenc_init_properties (GstMfxEnc * encode);

gboolean
gst_mfxenc_class_init_properties (GstMfxEncClass * encode_class);

G_END_DECLS
#endif /* GST_MFXENC_H */
