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

#include <gst/gst.h>
#include <gst/video/gstvideodecoder.h>

#include "gstmfxdecoder.h"
#include "gstmfxpluginbase.h"

G_BEGIN_DECLS

#define GST_TYPE_MFXDEC \
	(gst_mfxdec_get_type ())
#define GST_MFXDEC_CAST(obj) \
	((GstMfxDec *)(obj))
#define GST_MFXDEC(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MFXDEC, GstMfxDec))
#define GST_MFXDEC_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MFXDEC, GstMfxDecClass))
#define GST_MFXDEC_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_MFXDEC, GstMfxDecClass))
#define GST_IS_MFXDEC(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MFXDEC))
#define GST_IS_MFXDEC_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MFXDEC))

typedef struct _GstMfxDec GstMfxDec;
typedef struct _GstMfxDecClass GstMfxDecClass;

struct _GstMfxDec {
	/*< private >*/
	GstMfxPluginBase  parent_instance;

	GstCaps				*sinkpad_caps;
	GstCaps				*srcpad_caps;
	GstMfxDecoder		*decoder;
	guint                async_depth;

	GstVideoCodecState	*input_state;
	volatile gboolean	 active;
	volatile gboolean    do_renego;
};

struct _GstMfxDecClass {
	/*< private >*/
	GstMfxPluginBaseClass parent_class;
};

GType gst_mfxdec_get_type (void);

G_END_DECLS


#endif /* __GST_MFX_DEC_H__ */

