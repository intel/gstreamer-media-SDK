/*
 ============================================================================
 Name        : gst-mfx-trans.h
 Author      : Heiher <admin@heiher.info>
 Version     : 0.0.1
 Copyright   : Copyright (C) 2013 everyone.
 Description : 
 ============================================================================
 */

#ifndef __GST_MFX_TRANS_H__
#define __GST_MFX_TRANS_H__

#include <gst/gst.h>

#include "gst-mfx-base.h"

G_BEGIN_DECLS

#define GST_TYPE_MFX_TRANS (gst_mfx_trans_get_type ())
#define GST_MFX_TRANS(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MFX_TRANS, GstMfxTrans))
#define GST_IS_MFX_TRANS(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MFX_TRANS))
#define GST_MFX_TRANS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MFX_TRANS, GstMfxTransClass))
#define GST_IS_MFX_TRANS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MFX_TRANS))
#define GST_MFX_TRANS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_MFX_TRANS, GstMfxTransClass))

typedef struct _GstMfxTrans GstMfxTrans;
typedef struct _GstMfxTransClass GstMfxTransClass;

struct _GstMfxTrans
{
    GstMfxBase parent_instance;
};

struct _GstMfxTransClass
{
    GstMfxBaseClass parent_class;

    /* Virtual methods for subclass */
    void (*update_params) (GstMfxTrans *trans, mfxVideoParam *params);
};

GType gst_mfx_trans_get_type (void);

G_END_DECLS

#endif /* __GST_MFX_TRANS_H__ */

