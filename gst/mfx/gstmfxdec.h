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

#ifndef __GST_MFX_DEC_H__
#define __GST_MFX_DEC_H__

#include "gstmfxpluginbase.h"
#include <gst-libs/mfx/gstmfxdecoder.h>

G_BEGIN_DECLS

#define GST_MFXDEC(obj) ((GstMfxDec *)(obj))

typedef struct _GstMfxDec GstMfxDec;
typedef struct _GstMfxDecClass GstMfxDecClass;

struct _GstMfxDec {
  /*< private >*/
  GstMfxPluginBase     parent_instance;

  GstCaps             *sinkpad_caps;
  GstCaps             *srcpad_caps;
  GstMfxDecoder       *decoder;
  guint                async_depth;
  gboolean             live_mode;
  gboolean             skip_corrupted_frames;
  GstMfxSurface*       prev_surf;
  gboolean             dequeuing;
  gint                 flushing;

  GstVideoCodecState  *input_state;
  volatile gboolean    do_renego;
};

struct _GstMfxDecClass {
  /*< private >*/
  GstMfxPluginBaseClass parent_class;
};

gboolean gst_mfxdec_register (GstPlugin * plugin);


G_END_DECLS


#endif /* __GST_MFX_DEC_H__ */

