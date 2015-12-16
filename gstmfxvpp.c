/*
 ============================================================================
 Name        : gst-mfx-trans.c
 Author      : Heiher <admin@heiher.info>
 Version     : 0.0.1
 Copyright   : Copyright (C) 2013 everyone.
 Description : 
 ============================================================================
 */

#include "gst-mfx-helpers.h"
#include "gst-mfx-trans.h"

#define GST_FOURCC_NV12     GST_MAKE_FOURCC ('N', 'V', '1', '2')
#define GST_FOURCC_I420     GST_MAKE_FOURCC ('I', '4', '2', '0')

#define GST_MFX_TRANS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GST_TYPE_MFX_TRANS, GstMfxTransPrivate))
#define GST_CAT_DEFAULT (mfxtrans_debug)

typedef struct _GstMfxTransPrivate GstMfxTransPrivate;
typedef struct _GstMfxTransTask GstMfxTransTask;

struct _GstMfxTransPrivate
{
    GstPad *sink_pad;
    GstPad *src_pad;
    GstCaps *src_pad_caps;
    GstMfxTransTask *task_pool;
    GstMfxTransTask *task_curr;

    guint32 in_buf_len;
    guint32 out_buf_len;
    guint32 task_pool_len;

    GMutex exec_mutex;
    GCond exec_cond;
    GQueue exec_queue;
    GMutex idle_mutex;
    GCond idle_cond;
    GQueue idle_queue;

    mfxVideoParam mfx_video_param;

    GstFlowReturn src_pad_ret;
    gboolean src_pad_push_status;
};

struct _GstMfxTransTask
{
    mfxFrameSurface1 input;
    mfxFrameSurface1 output;
    mfxSyncPoint sp;

    GstClockTime duration;
};

static void gst_mfx_trans_push_exec_task (GstMfxTrans *self, GstMfxTransTask *task);
static GstMfxTransTask * gst_mfx_trans_pop_exec_task (GstMfxTrans *self);
static void gst_mfx_trans_push_idle_task (GstMfxTrans *self, GstMfxTransTask *task);
static GstMfxTransTask * gst_mfx_trans_pop_idle_task (GstMfxTrans *self);
static GstFlowReturn gst_mfx_trans_sync_task (GstMfxTrans *self, gboolean send);
static void gst_mfx_trans_flush_frames (GstMfxTrans *self, gboolean send);
static GstStateChangeReturn gst_mfx_trans_change_state (GstElement *element,
            GstStateChange transition);
static gboolean gst_mfx_trans_sink_pad_setcaps (GstPad *pad,
            GstCaps *caps);
static gboolean gst_mfx_trans_sink_pad_event (GstPad *pad, GstEvent *event);
static GstFlowReturn gst_mfx_trans_sink_pad_bufferalloc (GstPad *pad,
            guint64 offset, guint size, GstCaps *caps, GstBuffer **buf);
static GstFlowReturn gst_mfx_trans_sink_pad_chain (GstPad *pad,
            GstBuffer *buf);
static gboolean gst_mfx_trans_src_pad_activatepush (GstPad *pad,
            gboolean activate);
static void gst_mfx_trans_src_pad_task_handler (gpointer data);

GST_DEBUG_CATEGORY_STATIC (mfxtrans_debug);

static GstStaticPadTemplate gst_mfx_trans_sink_template =
GST_STATIC_PAD_TEMPLATE (
            "sink",
            GST_PAD_SINK,
            GST_PAD_ALWAYS,
            GST_STATIC_CAPS ("video/x-raw-yuv, "
                "format = (fourcc) { I420, YV12, NV12 }, "
                "framerate = (fraction) [0, MAX], "
                "width = (int) [ 16, MAX ], " "height = (int) [ 16, MAX ]; "
                "video/x-raw-rgb, "
                "bpp = (int) 32, " "depth = (int) 24, "
                "endianness = (int) BIG_ENDIAN, "
                "red_mask =   (int) 0xFF000000, "
                "green_mask = (int) 0x00FF0000, "
                "blue_mask =  (int) 0x0000FF00, "
                "width = (int) [ 16, MAX ], "
                "height = (int) [ 16, MAX ], "
                "framerate = (fraction) [0, MAX]; "
                "video/x-raw-rgb, "
                "bpp = (int) 32, " "depth = (int) 24, "
                "endianness = (int) BIG_ENDIAN, "
                "red_mask =   (int) 0x00FF0000, "
                "green_mask = (int) 0x0000FF00, "
                "blue_mask =  (int) 0x000000FF, "
                "width = (int) [ 16, MAX ], "
                "height = (int) [ 16, MAX ], "
                "framerate = (fraction) [0, MAX]; "
                "video/x-raw-rgb, "
                "bpp = (int) 32, " "depth = (int) 24, "
                "endianness = (int) BIG_ENDIAN, "
                "red_mask =   (int) 0x0000FF00, "
                "green_mask = (int) 0x00FF0000, "
                "blue_mask =  (int) 0xFF000000, "
                "width = (int) [ 16, MAX ], "
                "height = (int) [ 16, MAX ], "
                "framerate = (fraction) [0, MAX]; "
                "video/x-raw-rgb, "
                "bpp = (int) 32, " "depth = (int) 24, "
                "endianness = (int) BIG_ENDIAN, "
                "red_mask =   (int) 0x000000FF, "
                "green_mask = (int) 0x0000FF00, "
                "blue_mask =  (int) 0x00FF0000, "
                "width = (int) [ 16, MAX ], "
                "height = (int) [ 16, MAX ], "
                "framerate = (fraction) [0, MAX]")
            );

static GstStaticPadTemplate gst_mfx_trans_src_template =
GST_STATIC_PAD_TEMPLATE (
            "src",
            GST_PAD_SRC,
            GST_PAD_ALWAYS,
            GST_STATIC_CAPS ("video/x-raw-yuv, "
                "format = (fourcc) { NV12 }, "
                "framerate = (fraction) [0, MAX], "
                "width = (int) [ 16, MAX ], " "height = (int) [ 16, MAX ]")
            );

//GST_BOILERPLATE (GstMfxTrans, gst_mfx_trans,
//            GstMfxBase, GST_TYPE_MFX_BASE);

G_DEFINE_TYPE(GstMfxTrans, gst_mfx_trans, GST_TYPE_MFX_BASE);

static void
gst_mfx_trans_dispose (GObject *obj)
{
    G_OBJECT_CLASS (gst_mfx_trans_parent_class)->dispose (obj);
}

static void
gst_mfx_trans_finalize (GObject *obj)
{
    GstMfxTrans *self = GST_MFX_TRANS (obj);
    GstMfxTransPrivate *priv = GST_MFX_TRANS_GET_PRIVATE (self);

    g_queue_clear (&priv->idle_queue);
    g_cond_clear (&priv->idle_cond);
    g_mutex_clear (&priv->idle_mutex);
    g_queue_clear (&priv->exec_queue);
    g_cond_clear (&priv->exec_cond);
    g_mutex_clear (&priv->exec_mutex);

    if (priv->task_pool) {
        guint i = 0;

        for (i=0; i<priv->task_pool_len; i++) {
            g_slice_free1 (priv->in_buf_len,
                        priv->task_pool[i].input.Data.MemId);
            g_slice_free1 (priv->out_buf_len,
                        priv->task_pool[i].output.Data.MemId);
        }
        g_slice_free1 (sizeof (GstMfxTransTask) * priv->task_pool_len,
                    priv->task_pool);
        priv->task_pool_len = 0;
        priv->task_pool = NULL;
    }

    if (priv->src_pad_caps) {
        gst_caps_unref (priv->src_pad_caps);
        priv->src_pad_caps = NULL;
    }

    G_OBJECT_CLASS (gst_mfx_trans_parent_class)->finalize (obj);
}

static GObject *
gst_mfx_trans_constructor (GType type,
            guint n,
            GObjectConstructParam *param)
{
    return G_OBJECT_CLASS (gst_mfx_trans_parent_class)->constructor (type, n, param);
}

static void
gst_mfx_trans_constructed (GObject *obj)
{
    guint16 io_pattern = 0;

    G_OBJECT_CLASS (gst_mfx_trans_parent_class)->constructed (obj);

    /* Default IOPattern for scaler */
    g_object_get (obj, "io-pattern", &io_pattern, NULL);
    if (0 == io_pattern)
      g_object_set (obj,
                  "io-pattern", (MFX_IOPATTERN_IN_SYSTEM_MEMORY |
                      MFX_IOPATTERN_OUT_SYSTEM_MEMORY),
                  NULL);
}

static void
gst_mfx_trans_base_init (gpointer klass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

    gst_element_class_add_pad_template (element_class,
            gst_static_pad_template_get (&gst_mfx_trans_sink_template));
    gst_element_class_add_pad_template (element_class,
            gst_static_pad_template_get (&gst_mfx_trans_src_template));
    gst_element_class_set_details_simple (element_class,
                "MFX Transform",
                "Converter/Video",
                "MFX Video Transform",
                "Heiher <admin@heiher.info>");
}

static void
gst_mfx_trans_class_init (GstMfxTransClass *klass)
{
    GObjectClass *obj_class = G_OBJECT_CLASS (klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

    obj_class->constructor = gst_mfx_trans_constructor;
    obj_class->constructed = gst_mfx_trans_constructed;
    obj_class->dispose = gst_mfx_trans_dispose;
    obj_class->finalize = gst_mfx_trans_finalize;

    element_class->change_state = GST_DEBUG_FUNCPTR (gst_mfx_trans_change_state);

    g_type_class_add_private (klass, sizeof (GstMfxTransPrivate));

    GST_DEBUG_CATEGORY_INIT (mfxtrans_debug, "mfxtrans", 0, "MFX Transform");
}

static void
gst_mfx_trans_init (GstMfxTrans *self,
            GstMfxTransClass *klass)
{
    GstMfxTransPrivate *priv = GST_MFX_TRANS_GET_PRIVATE (self);

    g_queue_init (&priv->exec_queue);
    g_cond_init (&priv->exec_cond);
    g_mutex_init (&priv->exec_mutex);
    g_queue_init (&priv->idle_queue);
    g_cond_init (&priv->idle_cond);
    g_mutex_init (&priv->idle_mutex);

    priv->src_pad_ret = GST_FLOW_OK;

    gst_element_create_all_pads (GST_ELEMENT (self));
    
    priv->sink_pad = gst_element_get_static_pad (
                GST_ELEMENT (self), "sink");
    priv->src_pad = gst_element_get_static_pad (
                GST_ELEMENT (self), "src");

    gst_pad_set_setcaps_function (priv->sink_pad,
                GST_DEBUG_FUNCPTR (gst_mfx_trans_sink_pad_setcaps));
    gst_pad_set_event_function (priv->sink_pad,
                GST_DEBUG_FUNCPTR (gst_mfx_trans_sink_pad_event));
    gst_pad_set_bufferalloc_function (priv->sink_pad,
                GST_DEBUG_FUNCPTR (gst_mfx_trans_sink_pad_bufferalloc));
    gst_pad_set_chain_function (priv->sink_pad,
                GST_DEBUG_FUNCPTR (gst_mfx_trans_sink_pad_chain));

    gst_pad_set_activatepush_function (priv->src_pad,
                GST_DEBUG_FUNCPTR (gst_mfx_trans_src_pad_activatepush));
}

static void
gst_mfx_trans_push_exec_task (GstMfxTrans *self, GstMfxTransTask *task)
{
    GstMfxTransPrivate *priv = GST_MFX_TRANS_GET_PRIVATE (self);

    g_return_if_fail (NULL != task);

    g_mutex_lock (&priv->exec_mutex);
    g_queue_push_tail (&priv->exec_queue, task);
    g_cond_signal (&priv->exec_cond);
    g_mutex_unlock (&priv->exec_mutex);
}

static GstMfxTransTask *
gst_mfx_trans_pop_exec_task (GstMfxTrans *self)
{
    GstMfxTransPrivate *priv = GST_MFX_TRANS_GET_PRIVATE (self);
    GstMfxTransTask *task = NULL;

    g_mutex_lock (&priv->exec_mutex);
    while (priv->src_pad_push_status &&
                !g_queue_peek_head (&priv->exec_queue))
      g_cond_wait (&priv->exec_cond, &priv->exec_mutex);
    task = g_queue_pop_head (&priv->exec_queue);
    g_mutex_unlock (&priv->exec_mutex);

    return task;
}

static void
gst_mfx_trans_push_idle_task (GstMfxTrans *self, GstMfxTransTask *task)
{
    GstMfxTransPrivate *priv = GST_MFX_TRANS_GET_PRIVATE (self);

    g_return_if_fail (NULL != task);

    task->sp = NULL;
    task->input.Data.Locked = 0;
    task->output.Data.Locked = 0;

    g_mutex_lock (&priv->idle_mutex);
    g_queue_push_tail (&priv->idle_queue, task);
    g_cond_signal (&priv->idle_cond);
    g_mutex_unlock (&priv->idle_mutex);
}

static GstMfxTransTask *
gst_mfx_trans_pop_idle_task (GstMfxTrans *self)
{
    GstMfxTransPrivate *priv = GST_MFX_TRANS_GET_PRIVATE (self);
    GstMfxTransTask *task = NULL;

    g_mutex_lock (&priv->idle_mutex);
    while (priv->src_pad_push_status &&
                !g_queue_peek_head (&priv->idle_queue))
      g_cond_wait (&priv->idle_cond, &priv->idle_mutex);
    task = g_queue_pop_head (&priv->idle_queue);
    g_mutex_unlock (&priv->idle_mutex);

    return task;
}

static GstFlowReturn
gst_mfx_trans_sync_task (GstMfxTrans *self, gboolean send)
{
    GstMfxBase *parent = GST_MFX_BASE (self);
    GstMfxTransPrivate *priv = GST_MFX_TRANS_GET_PRIVATE (self);
    GstFlowReturn ret = GST_FLOW_OK;
    GstMfxTransTask *task = NULL;
    mfxStatus s = MFX_ERR_NONE;

    /* Pop task from exec queue */
    task = gst_mfx_trans_pop_exec_task (self);
    if (G_UNLIKELY (NULL == task))
      return GST_FLOW_EOS;
    do {
        s = MFXVideoCORE_SyncOperation (parent->mfx_session,
                    task->sp, G_MAXUINT32);
        /* The async operation is ready, push to src pad */
        if (MFX_ERR_NONE == s) {
            GstBuffer *buffer = NULL;

            if (send) {
                ret = gst_pad_alloc_buffer (priv->src_pad,
                            GST_BUFFER_OFFSET_NONE,
                            priv->out_buf_len,
                            priv->src_pad_caps,
                            &buffer);
                if (GST_FLOW_OK == ret && NULL != buffer) {
                    memcpy (GST_BUFFER_DATA (buffer),
                                task->output.Data.MemId,
                                priv->out_buf_len);
                    GST_BUFFER_OFFSET (buffer) = GST_BUFFER_OFFSET_NONE;
                    GST_BUFFER_TIMESTAMP (buffer) = task->output.Data.TimeStamp;
                    GST_BUFFER_DURATION (buffer) = task->duration;

                    ret = gst_pad_push (priv->src_pad, buffer);
                }
            }
            
            /* Push task to idle queue */
            gst_mfx_trans_push_idle_task (self, task);
        } else if (MFX_ERR_NONE > s) {
            /* Push task to idle queue */
            gst_mfx_trans_push_idle_task (self, task);
        }
    } while (MFX_ERR_NONE < s);

    return ret;
}

static void
gst_mfx_trans_flush_frames (GstMfxTrans *self, gboolean send)
{
    GstMfxTransPrivate *priv = GST_MFX_TRANS_GET_PRIVATE (self);

    while (g_queue_peek_head (&priv->exec_queue))
      gst_mfx_trans_sync_task (self, send);
}

static GstStateChangeReturn
gst_mfx_trans_change_state (GstElement *element,
            GstStateChange transition)
{
    GstMfxTrans *self = GST_MFX_TRANS (element);
    GstMfxBase *parent = GST_MFX_BASE (self);
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

    if (GST_STATE_CHANGE_DIR_UPWARDS ==
            GST_STATE_CHANGE_DIR (transition)) {
        ret = GST_ELEMENT_CLASS (gst_mfx_trans_parent_class)->change_state (element, transition);
        if (GST_STATE_CHANGE_FAILURE == ret)
          goto out;
    }

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        gst_mfx_trans_flush_frames (self, FALSE);
        MFXVideoVPP_Close (parent->mfx_session);
        break;
    case GST_STATE_CHANGE_READY_TO_NULL:
        break;
    default:
        break;
    }

    if (GST_STATE_CHANGE_DIR_DOWNWARDS ==
            GST_STATE_CHANGE_DIR (transition)) {
        ret = GST_ELEMENT_CLASS (gst_mfx_trans_parent_class)->change_state (element, transition);
        if (GST_STATE_CHANGE_FAILURE == ret)
          goto out;
    }

out:
    return ret;
}

static gboolean
gst_mfx_trans_sink_pad_setcaps (GstPad *pad, GstCaps *caps)
{
    GstMfxTrans *self = GST_MFX_TRANS (GST_OBJECT_PARENT (pad));
    GstMfxTransPrivate *priv = GST_MFX_TRANS_GET_PRIVATE (self);
    GstMfxTransClass *mfx_trans_class = GST_MFX_TRANS_GET_CLASS (self);
    GstMfxBase *parent = GST_MFX_BASE (self);
    GstStructure *structure = NULL;
    mfxStatus s = MFX_ERR_NONE;
    mfxFrameAllocRequest reqs[2];
    gint width = 0, height = 0, width_out = 0, height_out = 0;
    gint numerator = 0, denominator = 0,
         red_mask = 0, green_mask = 0, blue_mask = 0;
    guint32 real_format = 0, format = 0;

    if (!GST_CAPS_IS_SIMPLE (caps))
      goto fail;

    if (priv->task_pool)
      gst_mfx_trans_flush_frames (self, TRUE);

    structure = gst_caps_get_structure (caps, 0);
    if (!structure)
      goto fail;
    if (!gst_structure_get (structure,
                    "width", G_TYPE_INT, &width,
                    "height", G_TYPE_INT, &height,
                    NULL))
      goto fail;
    if (!gst_structure_get_fraction (structure, "framerate",
                    &numerator, &denominator))
      goto fail;
    if (!gst_structure_get_fourcc (structure,
                    "format", &real_format) &&
        !gst_structure_get (structure,
                    "red_mask", G_TYPE_INT, &red_mask,
                    "green_mask", G_TYPE_INT, &green_mask,
                    "blue_mask", G_TYPE_INT, &blue_mask,
                    NULL))
      goto fail;
    if (0 == real_format)
      format = real_format = MFX_FOURCC_RGB4;
    else if (GST_FOURCC_I420 == real_format)
      format = MFX_FOURCC_YV12;
    else
      format = real_format;

    memset (&priv->mfx_video_param, 0, sizeof (mfxVideoParam));
    g_object_get (self,
                "async-depth", &priv->mfx_video_param.AsyncDepth,
                "protected", &priv->mfx_video_param.Protected,
                "io-pattern", &priv->mfx_video_param.IOPattern,
                NULL);
    priv->mfx_video_param.vpp.In.Width = width;
    priv->mfx_video_param.vpp.In.Height = height;
    priv->mfx_video_param.vpp.In.FrameRateExtD = denominator;
    priv->mfx_video_param.vpp.In.FrameRateExtN = numerator;
    priv->mfx_video_param.vpp.In.FourCC = format;
    priv->mfx_video_param.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    priv->mfx_video_param.vpp.In.CropX = 0;
    priv->mfx_video_param.vpp.In.CropY = 0;
    priv->mfx_video_param.vpp.In.CropW = width;
    priv->mfx_video_param.vpp.In.CropH = height;
    priv->mfx_video_param.vpp.Out.Width = width;
    priv->mfx_video_param.vpp.Out.Height = height;
    priv->mfx_video_param.vpp.Out.FrameRateExtD = denominator;
    priv->mfx_video_param.vpp.Out.FrameRateExtN = numerator;
    priv->mfx_video_param.vpp.Out.FourCC = MFX_FOURCC_NV12;
    priv->mfx_video_param.vpp.Out.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    priv->mfx_video_param.vpp.Out.CropX = 0;
    priv->mfx_video_param.vpp.Out.CropY = 0;
    priv->mfx_video_param.vpp.Out.CropW = width;
    priv->mfx_video_param.vpp.Out.CropH = height;
    /* Call subclass to update params */
    if (mfx_trans_class->update_params)
      mfx_trans_class->update_params (self, &priv->mfx_video_param);
    s = MFXVideoVPP_Init (parent->mfx_session, &priv->mfx_video_param);
    if (MFX_ERR_NONE != s) {
        GST_ERROR ("MFXVideoVPP_Init failed(%d)!", s);
        goto fail;
    }

    width_out = priv->mfx_video_param.vpp.Out.Width;
    height_out = priv->mfx_video_param.vpp.Out.Height;

    s = MFXVideoVPP_GetVideoParam (parent->mfx_session, &priv->mfx_video_param);
    if (MFX_ERR_NONE != s) {
        GST_ERROR ("MFXVideoVPP_GetVideoParam failed(%d)!", s);
        goto fail;
    }
    /* Calc input buffer length */
    switch (real_format) {
    case MFX_FOURCC_NV12:
    case MFX_FOURCC_YV12:
    case GST_FOURCC_I420:
        priv->in_buf_len = width * height +
            (width>>1) * (height>>1) +
            (width>>1) * (height>>1);
        break;
    case MFX_FOURCC_RGB4:
        priv->in_buf_len = width * height * 4;
        break;
    }
    /* Calc output buffer length */
    switch (priv->mfx_video_param.vpp.Out.FourCC) {
    case MFX_FOURCC_NV12:
        priv->out_buf_len = width_out * height_out +
            (width_out>>1) * (height_out>>1) +
            (width_out>>1) * (height_out>>1);
        break;
    }

    s = MFXVideoVPP_QueryIOSurf (parent->mfx_session,
                &priv->mfx_video_param, reqs);
    if (MFX_ERR_NONE != s) {
        GST_ERROR ("MFXVideoVPP_QueryIOSurf failed(%d)!", s);
        goto fail;
    }
    /* Free previous task pool */
    if (priv->task_pool) {
        guint i = 0;

        for (i=0; i<priv->task_pool_len; i++) {
            g_slice_free1 (priv->in_buf_len,
                        priv->task_pool[i].input.Data.MemId);
            g_slice_free1 (priv->out_buf_len,
                        priv->task_pool[i].output.Data.MemId);
        }
        g_slice_free1 (sizeof (GstMfxTransTask) * priv->task_pool_len,
                    priv->task_pool);

        while (g_queue_pop_head (&priv->exec_queue));
        while (g_queue_pop_head (&priv->idle_queue));
    }
    /* Alloc new task pool */
    priv->task_pool_len = (reqs[0].NumFrameSuggested +
                reqs[1].NumFrameSuggested) / 2;
    priv->task_pool = g_slice_alloc0 (sizeof (GstMfxTransTask) * priv->task_pool_len);
    {
        guint i = 0;

        for (i=0; i<priv->task_pool_len; i++) {
            GstMfxTransTask *task = &priv->task_pool[i];

            /* Set input frame info */
            memcpy (&task->input.Info,
                        &priv->mfx_video_param.vpp.In,
                        sizeof (mfxFrameInfo));
            /* Set output frame info */
            memcpy (&task->output.Info,
                        &priv->mfx_video_param.vpp.Out,
                        sizeof (mfxFrameInfo));
            /* Alloc buffer for input: mfxFrameSurface1 */
            task->input.Data.MemId =
                g_slice_alloc0 (priv->in_buf_len);
            switch (real_format) {
            case MFX_FOURCC_NV12:
                task->input.Data.Y = task->input.Data.MemId;
                task->input.Data.U = task->input.Data.Y + width * height;
                task->input.Data.V = task->input.Data.U + 1;
                task->input.Data.Pitch = width;
                break;
            case MFX_FOURCC_YV12:
                task->input.Data.Y = task->input.Data.MemId;
                task->input.Data.V = task->input.Data.Y + width * height;
                task->input.Data.U = task->input.Data.V + (width * height >> 2);
                task->input.Data.Pitch = width;
                break;
            case GST_FOURCC_I420:
                task->input.Data.Y = task->input.Data.MemId;
                task->input.Data.U = task->input.Data.Y + width * height;
                task->input.Data.V = task->input.Data.U + (width * height >> 2);
                task->input.Data.Pitch = width;
                break;
            /*case MFX_FOURCC_RGB4:
                {
                    guint offset = 0;
                    for (offset=0; offset<4; offset++) {
                        gint32 mask = 0xFF << (offset * 8);
                        gpointer data = task->input.Data.MemId;

                        data += 3 - offset;
                        if (mask == red_mask)
                          task->input.Data.R = data;
                        else if (mask == green_mask)
                          task->input.Data.G = data;
                        else if (mask == blue_mask)
                          task->input.Data.B = data;
                    }
                    task->input.Data.Pitch = width * 4;
                }
                break;*/
            }
            /* Alloc buffer for output: mfxFrameSurface1 */
            task->output.Data.MemId = g_slice_alloc0 (priv->out_buf_len);
            switch (priv->mfx_video_param.vpp.Out.FourCC) {
            case MFX_FOURCC_NV12:
                task->output.Data.Y = task->output.Data.MemId;
                task->output.Data.U = task->output.Data.Y + width_out * height_out;
                task->output.Data.V = task->output.Data.U + 1;
                task->output.Data.Pitch = width_out;
                break;
            }

            /* Push task to idle queue */
            gst_mfx_trans_push_idle_task (self, task);
        }
    }

    if (priv->src_pad_caps)
      gst_caps_unref (priv->src_pad_caps);
    structure = gst_structure_new ("video/x-raw-yuv",
                "width", G_TYPE_INT, width_out,
                "height", G_TYPE_INT, height_out,
                "framerate", GST_TYPE_FRACTION, numerator, denominator,
				"format", G_TYPE_STRING, GST_FOURCC_NV12,
                NULL);
    priv->src_pad_caps = gst_caps_new_full (structure, NULL);

    return TRUE;

fail:

    return FALSE;
}

static gboolean
gst_mfx_trans_sink_pad_event (GstPad *pad, GstEvent *event)
{
    GstMfxTrans *self = GST_MFX_TRANS (GST_OBJECT_PARENT (pad));
    GstMfxTransPrivate *priv = GST_MFX_TRANS_GET_PRIVATE (self);

    switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
        gst_mfx_trans_flush_frames (self, TRUE);
        break;
    default:
        break;
    }

    return gst_pad_push_event (priv->src_pad, event);
}

static GstFlowReturn
gst_mfx_trans_sink_pad_bufferalloc (GstPad *pad, guint64 offset,
            guint size, GstCaps *caps, GstBuffer **buf)
{
    GstMfxTrans *self = GST_MFX_TRANS (GST_OBJECT_PARENT (pad));
    GstMfxTransPrivate *priv = GST_MFX_TRANS_GET_PRIVATE (self);
    GstMfxTransTask *task = NULL;

    if (G_UNLIKELY (GST_FLOW_OK != priv->src_pad_ret))
      return priv->src_pad_ret;

    /* No task pool, alloc a normal buffer */
    if (G_UNLIKELY (!priv->task_pool)) {
        *buf = gst_buffer_new_and_alloc (size);
        if (!*buf)
          return GST_FLOW_ERROR;
        GST_BUFFER_OFFSET (*buf) = offset;
        
		//gst_buffer_set_caps (*buf, caps);

        return GST_FLOW_OK;
    }

    if (G_UNLIKELY (size != priv->in_buf_len))
      g_assert_not_reached ();

    task = gst_mfx_trans_pop_idle_task (self);
    if (NULL == task)
      return GST_FLOW_ERROR;

    /**buf = gst_buffer_new ();
    if (!*buf)
      return GST_FLOW_ERROR;
    GST_BUFFER_DATA (*buf) = task->input.Data.MemId;
    GST_BUFFER_SIZE (*buf) = priv->in_buf_len;
    GST_BUFFER_OFFSET (*buf) = GST_BUFFER_OFFSET_NONE;
    gst_buffer_set_caps (*buf, caps);*/

	*buf = gst_buffer_new_wrapped(task->input.Data.MemId, priv->in_buf_len);
	if (!*buf)
		return GST_FLOW_ERROR;

	GST_BUFFER_OFFSET(*buf) = GST_BUFFER_OFFSET_NONE;

    /* Save the task in task_curr */
    priv->task_curr = task;

    return GST_FLOW_OK;
}

static GstFlowReturn
gst_mfx_trans_sink_pad_chain (GstPad *pad, GstBuffer *buf)
{
    GstMfxTrans *self = GST_MFX_TRANS (GST_OBJECT_PARENT (pad));
    GstMfxTransPrivate *priv = GST_MFX_TRANS_GET_PRIVATE (self);
    GstMfxBase *parent = GST_MFX_BASE (self);
    GstMfxTransTask *task = NULL;
    GstFlowReturn ret = GST_FLOW_OK;
    gboolean retry = TRUE, mcpy = FALSE;

    /* Guess the input buf is in task_curr */
    if (priv->task_curr && GST_BUFFER_DATA (buf) ==
                priv->task_curr->input.Data.MemId) {
        task = priv->task_curr;
        priv->task_curr = NULL;
    } else {
        gint i = 0;

        /* Oh, is wrong! find buf's owner: task */
        for (i=0; i<priv->task_pool_len; i++) {
            if (priv->task_pool[i].input.Data.MemId ==
                        GST_BUFFER_DATA (buf)) {
                task = &priv->task_pool[i];
                break;
            }
        }
    }

    /* Not found in task pool, may be is first alloced buffer.
     * Get a idle task to handle it and set mcpy = TRUE.
     */
    if (NULL == task) {
        task = gst_mfx_trans_pop_idle_task (self);
        if (NULL == task)
          goto fail;
        mcpy = TRUE;
    }

    /* Input: mfxFrameSurface1 */
    if (G_UNLIKELY (mcpy)) {
        if (priv->in_buf_len != GST_BUFFER_SIZE (buf))
          g_assert_not_reached ();
        memcpy (task->input.Data.MemId,
                    GST_BUFFER_DATA (buf), priv->in_buf_len);
    }
    task->input.Data.TimeStamp = GST_BUFFER_TIMESTAMP (buf);

    /* Output: save duration */
    task->duration = GST_BUFFER_DURATION (buf);

    /* Free input buffer: GstBuffer */
    gst_buffer_unref (buf);

    /* Commit the task to MFX VPP */
    do {
        mfxStatus s = MFX_ERR_NONE;

        s = MFXVideoVPP_RunFrameVPPAsync (parent->mfx_session,
                    &task->input, &task->output, NULL, &task->sp);

        if (MFX_ERR_NONE < s && !task->sp) {
            if (MFX_WRN_DEVICE_BUSY == s)
              g_usleep (100);
            retry = TRUE;
        } else if (MFX_ERR_NONE < s && task->sp) {
            retry = FALSE;
        } else if (MFX_ERR_MORE_DATA == s) {
            retry = TRUE;
        } else if (MFX_ERR_NONE != s) {
            GST_ERROR ("MFXVideoVPP_RunFrameVPPAsync failed(%d)!", s);
            ret = GST_FLOW_ERROR;
            goto fail;
        } else {
            retry = FALSE;
        }
    } while (retry);

    /* Push task to exec queue */
    gst_mfx_trans_push_exec_task (self, task);

    return ret;

fail:

    return ret;
}

static gboolean
gst_mfx_trans_src_pad_activatepush (GstPad *pad, gboolean activate)
{
    GstMfxTrans *self = GST_MFX_TRANS (GST_OBJECT_PARENT (pad));
    GstMfxTransPrivate *priv = GST_MFX_TRANS_GET_PRIVATE (self);
    gboolean ret = TRUE;

    if (activate) {
        priv->src_pad_push_status = TRUE;
        ret = gst_pad_start_task (priv->src_pad,
                    gst_mfx_trans_src_pad_task_handler, self, NULL);
    } else {
        /* Send a quit signal to task thread */
        g_mutex_lock (&priv->exec_mutex);
        priv->src_pad_push_status = FALSE;
        g_cond_signal (&priv->exec_cond);
        g_mutex_unlock (&priv->exec_mutex);
        ret = gst_pad_stop_task (priv->src_pad);
    }

    return ret;
}

static void
gst_mfx_trans_src_pad_task_handler (gpointer data)
{
    GstMfxTrans *self = GST_MFX_TRANS (data);
    GstMfxTransPrivate *priv = GST_MFX_TRANS_GET_PRIVATE (self);

    priv->src_pad_ret = gst_mfx_trans_sync_task (self, TRUE);
    if (G_UNLIKELY (GST_FLOW_OK != priv->src_pad_ret))
      gst_pad_pause_task (priv->src_pad);
}

