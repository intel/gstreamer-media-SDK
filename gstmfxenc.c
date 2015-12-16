/*
 ============================================================================
 Name        : gst-mfx-enc.c
 Author      : Ishmael Visayana Sameen <ishmael.visayana.sameen@intel.com>
 Version     : 0.0.1
 Copyright   : Copyright (C) 2015.
 Description : 
 ============================================================================
 */

#include "gst-mfx-helpers.h"
#include "gst-mfx-enc.h"

#define GST_MFX_ENC_CODEC_ID_DEFAULT            MFX_CODEC_AVC
#define GST_MFX_ENC_CODEC_PROFILE_DEFAULT       MFX_PROFILE_AVC_MAIN
#define GST_MFX_ENC_CODEC_LEVEL_DEFAULT         0
#define GST_MFX_ENC_NUM_THREAD_DEFAULT          0
#define GST_MFX_ENC_TARGET_USAGE_DEFAULT        MFX_TARGETUSAGE_BEST_SPEED
#define GST_MFX_ENC_GOP_PIC_SIZE_DEFAULT        24
#define GST_MFX_ENC_GOP_REF_DIST_DEFAULT        1
#define GST_MFX_ENC_GOP_OPT_FLAG_DEFAULT        MFX_GOP_CLOSED
#define GST_MFX_ENC_IDR_INTERVAL_DEFAULT        0
#define GST_MFX_ENC_RATE_CTL_METHOD_DEFAULT     MFX_RATECONTROL_CBR
#define GST_MFX_ENC_INIT_DELAY_DEFAULT          0
#define GST_MFX_ENC_BITRATE_DEFAULT             2048
#define GST_MFX_ENC_MAX_BITRATE_DEFAULT         0
#define GST_MFX_ENC_NUM_SLICE_DEFAULT           0
#define GST_MFX_ENC_NUM_REF_FRAME_DEFAULT       0
#define GST_MFX_ENC_ENCODED_ORDER_DEFAULT       0

#define GST_MFX_ENC_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GST_TYPE_MFX_ENC, GstMfxEncPrivate))
#define GST_CAT_DEFAULT (mfxenc_debug)

enum
{
    PROP_ZERO,
    PROP_CODEC_ID,
    PROP_CODEC_PROFILE,
    PROP_CODEC_LEVEL,
    PROP_NUM_THREAD,
    PROP_TARGET_USAGE,
    PROP_GOP_PIC_SIZE,
    PROP_GOP_REF_DIST,
    PROP_GOP_OPT_FLAG,
    PROP_IDR_INTERVAL,
    PROP_RATE_CTL_METHOD,
    PROP_INIT_DELAY,
    PROP_BITRATE,
    PROP_MAX_BITRATE,
    PROP_NUM_SLICE,
    PROP_NUM_REF_FRAME,
    PROP_ENCODED_ORDER,
    N_PROPERTIES
};

typedef struct _GstMfxEncPrivate GstMfxEncPrivate;
typedef struct _GstMfxEncTask GstMfxEncTask;

struct _GstMfxEncPrivate
{
    guint32 codec_id;
    guint16 codec_profile;
    guint16 codec_level;
    guint16 num_thread;
    guint16 target_usage;
    guint16 gop_pic_size;
    guint16 gop_ref_dist;
    guint16 gop_opt_flag;
    guint16 idr_interval;
    guint16 rate_ctl_method;
    guint16 init_delay;
    guint16 bitrate;
    guint16 max_bitrate;
    guint16 num_slice;
    guint16 num_ref_frame;
    guint16 encoded_order;

    GstPad *sink_pad;
    GstPad *src_pad;
    GstCaps *src_pad_caps;
    GstMfxEncTask *task_pool;
    GstMfxEncTask *task_curr;

    guint32 fs_buf_len;
    guint32 bs_buf_len;
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

struct _GstMfxEncTask
{
    mfxFrameSurface1 input;
    mfxBitstream output;
    mfxSyncPoint sp;

    GstClockTime duration;
};

static void gst_mfx_enc_push_exec_task (GstMfxEnc *self, GstMfxEncTask *task);
static GstMfxEncTask * gst_mfx_enc_pop_exec_task (GstMfxEnc *self);
static void gst_mfx_enc_push_idle_task (GstMfxEnc *self, GstMfxEncTask *task);
static GstMfxEncTask * gst_mfx_enc_pop_idle_task (GstMfxEnc *self);
static GstFlowReturn gst_mfx_enc_sync_task (GstMfxEnc *self, gboolean send);
static void gst_mfx_enc_flush_frames (GstMfxEnc *self, gboolean send);
static void gst_mfx_enc_set_property (GObject *obj, guint id,
            const GValue *value, GParamSpec *pspec);
static void gst_mfx_enc_get_property (GObject *obj, guint id,
            GValue *value, GParamSpec *pspec);
static GstStateChangeReturn gst_mfx_enc_change_state (GstElement *element,
            GstStateChange transition);


static gboolean gst_mfx_enc_setcaps (GstPad *pad, GstCaps *caps);
static gboolean gst_mfx_enc_sink_event (GstPad *pad, GstObject *parent, GstEvent *event);
static GstFlowReturn gst_mfx_enc_chain(GstPad *pad, GstObject * parent, GstBuffer *buf);
static gboolean gst_mfx_enc_src_activate_mode (GstPad *pad, GstObject *parent, GstPadMode mode, gboolean active);

// Replace with buffer pool
//static GstFlowReturn gst_mfx_enc_sink_pad_bufferalloc (GstPad *pad, guint64 offset, guint size, GstCaps *caps, GstBuffer **buf);

// gst_pad_push in _chain() function
//static gboolean gst_mfx_enc_src_pad_activatepush (GstPad *pad, gboolean activate);
static void gst_mfx_enc_src_pad_task_handler (gpointer data);

GST_DEBUG_CATEGORY_STATIC (mfxenc_debug);

static GstStaticPadTemplate gst_mfx_enc_sink_template =
GST_STATIC_PAD_TEMPLATE (
            "sink",
            GST_PAD_SINK,
            GST_PAD_ALWAYS,
            GST_STATIC_CAPS ("video/x-raw, "
                "format = (string) { NV12 }, "
                "framerate = (fraction) [0, MAX], "
                "width = (int) [ 16, MAX ], " "height = (int) [ 16, MAX ]")
            );

static GstStaticPadTemplate gst_mfx_enc_src_template =
GST_STATIC_PAD_TEMPLATE (
            "src",
            GST_PAD_SRC,
            GST_PAD_ALWAYS,
            GST_STATIC_CAPS ("video/x-h264, "
                "framerate = (fraction) [0/1, MAX], "
                "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ], "
                "stream-format = (string) { byte-stream, avc }, "
                "alignment = (string) { au }")
            );

//GST_BOILERPLATE (GstMfxEnc, gst_mfx_enc,
//            GstMfxBase, GST_TYPE_MFX_BASE);

G_DEFINE_TYPE(GstMfxEnc, gst_mfx_enc, GST_TYPE_MFX_BASE);

#define GST_TYPE_MFX_ENC_CODEC_ID   \
    (gst_mfx_enc_codec_id_get_type ())
static GType
gst_mfx_enc_codec_id_get_type (void)
{
    static GType codec_id_type = 0;
    static const GEnumValue codec_id[] =
    {
        { MFX_CODEC_AVC, "AVC", "avc" },
        { MFX_CODEC_MPEG2, "MPEG-2", "mpeg2" },
        { MFX_CODEC_VC1, "VC-1", "vc1" },
        { 0, NULL, NULL }
    };

    if (!codec_id_type)
      codec_id_type = g_enum_register_static ("GstMfxEncCodecIdType",
                  codec_id);

    return codec_id_type;
}

#define GST_TYPE_MFX_ENC_CODEC_PROFILE   \
    (gst_mfx_enc_codec_profile_get_type ())
static GType
gst_mfx_enc_codec_profile_get_type (void)
{
    static GType codec_profile_type = 0;
    static const GEnumValue codec_profile[] =
    {
        { MFX_PROFILE_UNKNOWN, "Unknown", "unknown" },
        { MFX_PROFILE_AVC_BASELINE, "AVC Baseline", "avc-baseline" },
        { MFX_PROFILE_AVC_MAIN, "AVC Main", "avc-main" },
        { MFX_PROFILE_AVC_HIGH, "AVC High", "avc-high" },
        { MFX_PROFILE_MPEG2_SIMPLE, "MPEG-2 Simple", "mpeg2-simple" },
        { MFX_PROFILE_MPEG2_MAIN, "MPEG-2 Main", "mpeg2-main" },
        { MFX_PROFILE_MPEG2_HIGH, "MPEG-2 High", "mpeg2-high" },
        { MFX_PROFILE_VC1_SIMPLE, "VC-1 Simple", "vc1-simple" },
        { MFX_PROFILE_VC1_MAIN, "VC-1 Main", "vc1-main" },
        { MFX_PROFILE_VC1_ADVANCED, "VC-1 Advanced", "vc1-advanced" },
        { 0, NULL, NULL }
    };

    if (!codec_profile_type)
      codec_profile_type = g_enum_register_static ("GstMfxEncCodecProfileType",
                  codec_profile);

    return codec_profile_type;
}

#define GST_TYPE_MFX_ENC_CODEC_LEVEL   \
    (gst_mfx_enc_codec_level_get_type ())
static GType
gst_mfx_enc_codec_level_get_type (void)
{
    static GType codec_level_type = 0;
    static const GEnumValue codec_level[] =
    {
        { MFX_LEVEL_UNKNOWN, "Unknown", "unknown" },
        { MFX_LEVEL_AVC_1, "AVC 1", "avc1" },
        { MFX_LEVEL_AVC_1b, "AVC 1b", "avc1b" },
        { MFX_LEVEL_AVC_11, "AVC 11", "avc11" },
        { MFX_LEVEL_AVC_12, "AVC 12", "avc12" },
        { MFX_LEVEL_AVC_13, "AVC 13", "avc13" },
        { MFX_LEVEL_AVC_2, "AVC 2", "avc2" },
        { MFX_LEVEL_AVC_21, "AVC 21", "avc21" },
        { MFX_LEVEL_AVC_22, "AVC 22", "avc22" },
        { MFX_LEVEL_AVC_3, "AVC 3", "avc3" },
        { MFX_LEVEL_AVC_31, "AVC 31", "avc31" },
        { MFX_LEVEL_AVC_32, "AVC 32", "avc32" },
        { MFX_LEVEL_AVC_4, "AVC 4", "avc4" },
        { MFX_LEVEL_AVC_41, "AVC 41", "avc41" },
        { MFX_LEVEL_AVC_42, "AVC 42", "avc42" },
        { MFX_LEVEL_AVC_5, "AVC 5", "avc5" },
        { MFX_LEVEL_AVC_51, "AVC 51", "avc51" },
        { MFX_LEVEL_AVC_52, "AVC 52", "avc52" },
        { MFX_LEVEL_MPEG2_LOW, "MPEG-2 Low", "mpeg2-low" },
        { MFX_LEVEL_MPEG2_MAIN, "MPEG-2 Main", "mpeg2-main" },
        { MFX_LEVEL_MPEG2_HIGH, "MPEG-2 High", "mpeg2-high" },
        { MFX_LEVEL_MPEG2_HIGH1440, "MPEG-2 High1440", "mpeg2-high1440" },
        { MFX_LEVEL_VC1_LOW, "VC-1 Low", "vc1-low" },
        { MFX_LEVEL_VC1_MEDIAN, "VC-1 Median", "vc1-median" },
        { MFX_LEVEL_VC1_HIGH, "VC-1 High", "vc1-high" },
        { 0, NULL, NULL }
    };

    if (!codec_level_type)
      codec_level_type = g_enum_register_static ("GstMfxEncCodecLevelType",
                  codec_level);

    return codec_level_type;
}

#define GST_TYPE_MFX_ENC_TARGET_USAGE   \
    (gst_mfx_enc_target_usage_get_type ())
static GType
gst_mfx_enc_target_usage_get_type (void)
{
    static GType target_usage_type = 0;
    static const GEnumValue target_usage[] =
    {
        { MFX_TARGETUSAGE_UNKNOWN, "Unknown", "unknown" },
        { MFX_TARGETUSAGE_BEST_QUALITY, "Best Quality", "best-quality" },
        { MFX_TARGETUSAGE_BALANCED, "Balanced", "balanced" },
        { MFX_TARGETUSAGE_BEST_SPEED, "Best Speed", "best-speed" },
        { 0, NULL, NULL }
    };

    if (!target_usage_type)
      target_usage_type = g_enum_register_static ("GstMfxEncTargetUsageType",
                  target_usage);

    return target_usage_type;
}

#define GST_TYPE_MFX_ENC_GOP_OPT_FLAG   \
    (gst_mfx_enc_gop_opt_flag_get_type ())
static GType
gst_mfx_enc_gop_opt_flag_get_type (void)
{
    static GType gop_opt_flag_type = 0;
    static const GEnumValue gop_opt_flag[] =
    {
        { MFX_GOP_CLOSED, "Gop Closed", "closed" },
        { MFX_GOP_STRICT, "Gop Strict", "strict" },
        { 0, NULL, NULL }
    };

    if (!gop_opt_flag_type)
      gop_opt_flag_type = g_enum_register_static ("GstMfxEncGopOptFlagType",
                  gop_opt_flag);

    return gop_opt_flag_type;
}

#define GST_TYPE_MFX_ENC_RATE_CTL_METHOD   \
    (gst_mfx_enc_rate_ctl_method_get_type ())
static GType
gst_mfx_enc_rate_ctl_method_get_type (void)
{
    static GType rate_ctl_method_type = 0;
    static const GEnumValue rate_ctl_method[] =
    {
        { MFX_RATECONTROL_CBR, "CBR", "cbr" },
        { MFX_RATECONTROL_VBR, "VBR", "vbr" },
        { MFX_RATECONTROL_CQP, "CQP", "cqp" },
        { MFX_RATECONTROL_AVBR, "AVBR", "avbr" },
        { 0, NULL, NULL }
    };

    if (!rate_ctl_method_type)
      rate_ctl_method_type = g_enum_register_static ("GstMfxEncRateCtlMethodType",
                  rate_ctl_method);

    return rate_ctl_method_type;
}

static void
gst_mfx_enc_dispose (GObject *obj)
{
    G_OBJECT_CLASS (gst_mfx_enc_parent_class)->dispose (obj);
}

static void
gst_mfx_enc_finalize (GObject *obj)
{
    GstMfxEnc *self = GST_MFX_ENC (obj);
    GstMfxEncPrivate *priv = GST_MFX_ENC_GET_PRIVATE (self);

    g_queue_clear (&priv->idle_queue);
    g_cond_clear (&priv->idle_cond);
    g_mutex_clear (&priv->idle_mutex);
    g_queue_clear (&priv->exec_queue);
    g_cond_clear (&priv->exec_cond);
    g_mutex_clear (&priv->exec_mutex);

    if (priv->task_pool) {
        guint i = 0;

        for (i=0; i<priv->task_pool_len; i++) {
            g_slice_free1 (priv->fs_buf_len,
                        priv->task_pool[i].input.Data.MemId);
            g_slice_free1 (priv->bs_buf_len,
                        priv->task_pool[i].output.Data);
        }
        g_slice_free1 (sizeof (GstMfxEncTask) * priv->task_pool_len,
                    priv->task_pool);
        priv->task_pool_len = 0;
        priv->task_pool = NULL;
    }

    if (priv->src_pad_caps) {
        gst_caps_unref (priv->src_pad_caps);
        priv->src_pad_caps = NULL;
    }

    G_OBJECT_CLASS (gst_mfx_enc_parent_class)->finalize (obj);
}

static GObject *
gst_mfx_enc_constructor (GType type,
            guint n,
            GObjectConstructParam *param)
{
    return G_OBJECT_CLASS (gst_mfx_enc_parent_class)->constructor (type, n, param);
}

static void
gst_mfx_enc_constructed (GObject *obj)
{
    guint16 io_pattern = 0;

    G_OBJECT_CLASS (gst_mfx_enc_parent_class)->constructed (obj);

    /* Default IOPattern for encoder */
    g_object_get (obj, "io-pattern", &io_pattern, NULL);
    if (0 == io_pattern)
      g_object_set (obj,
                  "io-pattern", MFX_IOPATTERN_IN_SYSTEM_MEMORY,
                  NULL);
}

static void
gst_mfx_enc_base_init(gpointer klass)
{
	GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

	gst_element_class_add_pad_template(element_class,
		gst_static_pad_template_get(&gst_mfx_enc_sink_template));
	gst_element_class_add_pad_template(element_class,
		gst_static_pad_template_get(&gst_mfx_enc_src_template));
	
	gst_element_class_set_static_metadata(element_class,
		"MFX Encoder",
		"Codec/Encoder/Video",
		"MFX Video Encoder",
		"Ishmael <ishmael.visayana.sameen@intel.com>");
}

static void
gst_mfx_enc_class_init (GstMfxEncClass *klass)
{
    GObjectClass *obj_class = G_OBJECT_CLASS (klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

    obj_class->constructor = gst_mfx_enc_constructor;
    obj_class->constructed = gst_mfx_enc_constructed;
    obj_class->set_property = gst_mfx_enc_set_property;
    obj_class->get_property = gst_mfx_enc_get_property;
    obj_class->dispose = gst_mfx_enc_dispose;
    obj_class->finalize = gst_mfx_enc_finalize;

    element_class->change_state = GST_DEBUG_FUNCPTR (gst_mfx_enc_change_state);
	
    g_object_class_install_property (obj_class, PROP_CODEC_ID,
                g_param_spec_enum ("codec-id", "Codec Id",
                    "Codec ID", GST_TYPE_MFX_ENC_CODEC_ID,
                    GST_MFX_ENC_CODEC_ID_DEFAULT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (obj_class, PROP_CODEC_PROFILE,
                g_param_spec_enum ("codec-profile", "Codec profile",
                    "Codec Profile", GST_TYPE_MFX_ENC_CODEC_PROFILE,
                    GST_MFX_ENC_CODEC_PROFILE_DEFAULT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (obj_class, PROP_CODEC_LEVEL,
                g_param_spec_enum ("codec-level", "Codec level",
                    "Codec Level", GST_TYPE_MFX_ENC_CODEC_LEVEL,
                    GST_MFX_ENC_CODEC_LEVEL_DEFAULT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (obj_class, PROP_NUM_THREAD,
                g_param_spec_uint ("num-thread", "Num thread",
                    "Num Thread", 0, G_MAXUINT,
                    GST_MFX_ENC_NUM_THREAD_DEFAULT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (obj_class, PROP_TARGET_USAGE,
                g_param_spec_enum ("target-usage", "Target usage",
                    "Target Usage", GST_TYPE_MFX_ENC_TARGET_USAGE,
                    GST_MFX_ENC_TARGET_USAGE_DEFAULT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (obj_class, PROP_GOP_PIC_SIZE,
                g_param_spec_uint ("gop-pic-size", "Gop pic size",
                    "Gop Pic Size", 0, G_MAXUINT,
                    GST_MFX_ENC_GOP_PIC_SIZE_DEFAULT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (obj_class, PROP_GOP_REF_DIST,
                g_param_spec_uint ("gop-ref-dist", "Gop ref dist",
                    "Gop Ref Dist", 0, G_MAXUINT,
                    GST_MFX_ENC_GOP_REF_DIST_DEFAULT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (obj_class, PROP_GOP_OPT_FLAG,
                g_param_spec_enum ("gop-opt-flag", "Gop opt flag",
                    "Gop Opt Flag", GST_TYPE_MFX_ENC_GOP_OPT_FLAG,
                    GST_MFX_ENC_GOP_OPT_FLAG_DEFAULT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (obj_class, PROP_IDR_INTERVAL,
                g_param_spec_uint ("idr-interval", "IDR interval",
                    "IDR Interval", 0, G_MAXUINT,
                    GST_MFX_ENC_IDR_INTERVAL_DEFAULT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (obj_class, PROP_RATE_CTL_METHOD,
                g_param_spec_enum ("rate-ctl-method", "Rate ctl method",
                    "Rate Ctl Method", GST_TYPE_MFX_ENC_RATE_CTL_METHOD,
                    GST_MFX_ENC_RATE_CTL_METHOD_DEFAULT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (obj_class, PROP_INIT_DELAY,
                g_param_spec_uint ("init-delay", "Init delay",
                    "Init Delay (in KB)", 0, G_MAXUINT,
                    GST_MFX_ENC_INIT_DELAY_DEFAULT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (obj_class, PROP_BITRATE,
                g_param_spec_uint ("bitrate", "Bitrate",
                    "Bitrate (in Kbps)", 0, G_MAXUINT,
                    GST_MFX_ENC_BITRATE_DEFAULT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (obj_class, PROP_MAX_BITRATE,
                g_param_spec_uint ("max-bitrate", "Max bitrate",
                    "Max bitrate (in Kbps)", 0, G_MAXUINT,
                    GST_MFX_ENC_MAX_BITRATE_DEFAULT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (obj_class, PROP_NUM_SLICE,
                g_param_spec_uint ("num-slice", "Num slice",
                    "Num slice", 0, G_MAXUINT,
                    GST_MFX_ENC_NUM_SLICE_DEFAULT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (obj_class, PROP_NUM_REF_FRAME,
                g_param_spec_uint ("num-ref-frame", "Num ref frame",
                    "Num Ref Frame", 0, G_MAXUINT,
                    GST_MFX_ENC_NUM_REF_FRAME_DEFAULT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (obj_class, PROP_ENCODED_ORDER,
                g_param_spec_uint ("encoded-order", "Encoded order",
                    "Encoded Order", 0, G_MAXUINT,
                    GST_MFX_ENC_ENCODED_ORDER_DEFAULT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_type_class_add_private (klass, sizeof (GstMfxEncPrivate));

    GST_DEBUG_CATEGORY_INIT (mfxenc_debug, "mfxenc", 0, "MFX Encoder");
}

static void
gst_mfx_enc_init (GstMfxEnc *self)
{
    GstMfxEncPrivate *priv = GST_MFX_ENC_GET_PRIVATE (self);

    g_mutex_init (&priv->exec_mutex);
    g_cond_init (&priv->exec_cond);
    g_queue_init (&priv->exec_queue);
    g_mutex_init (&priv->idle_mutex);
    g_cond_init (&priv->idle_cond);
    g_queue_init (&priv->idle_queue);

    priv->src_pad_ret = GST_FLOW_OK;

    gst_element_create_all_pads (GST_ELEMENT (self));
    
    priv->sink_pad = gst_element_get_static_pad (
                GST_ELEMENT (self), "sink");
    priv->src_pad = gst_element_get_static_pad (
                GST_ELEMENT (self), "src");

	gst_pad_set_event_function(priv->sink_pad,
		GST_DEBUG_FUNCPTR(gst_mfx_enc_sink_event));

	gst_pad_set_chain_function(priv->sink_pad,
		GST_DEBUG_FUNCPTR(gst_mfx_enc_chain));

	gst_pad_set_activatemode_function(priv->src_pad,
		GST_DEBUG_FUNCPTR(gst_mfx_enc_src_activate_mode));
    
	// TODO
	// gst_pad_set_bufferalloc_function (priv->sink_pad, GST_DEBUG_FUNCPTR (gst_mfx_enc_sink_pad_bufferalloc));

	// REMOVE
    // gst_pad_set_activatepush_function (priv->src_pad, GST_DEBUG_FUNCPTR (gst_mfx_enc_src_pad_activatepush));
}

static void
gst_mfx_enc_push_exec_task (GstMfxEnc *self, GstMfxEncTask *task)
{
    GstMfxEncPrivate *priv = GST_MFX_ENC_GET_PRIVATE (self);

    g_return_if_fail (NULL != task);

    g_mutex_lock (&priv->exec_mutex);
    g_queue_push_tail (&priv->exec_queue, task);
    g_cond_signal (&priv->exec_cond);
    g_mutex_unlock (&priv->exec_mutex);
}

static GstMfxEncTask *
gst_mfx_enc_pop_exec_task (GstMfxEnc *self)
{
    GstMfxEncPrivate *priv = GST_MFX_ENC_GET_PRIVATE (self);
    GstMfxEncTask *task = NULL;

    g_mutex_lock (&priv->exec_mutex);
    while (priv->src_pad_push_status &&
                !g_queue_peek_head (&priv->exec_queue))
      g_cond_wait (&priv->exec_cond, &priv->exec_mutex);
    task = g_queue_pop_head (&priv->exec_queue);
    g_mutex_unlock (&priv->exec_mutex);

    return task;
}

static void
gst_mfx_enc_push_idle_task (GstMfxEnc *self, GstMfxEncTask *task)
{
    GstMfxEncPrivate *priv = GST_MFX_ENC_GET_PRIVATE (self);

    g_return_if_fail (NULL != task);

    task->sp = NULL;
    task->input.Data.Locked = 0;
    task->output.DataOffset = 0;
    task->output.DataLength = 0;

    g_mutex_lock (&priv->idle_mutex);
    g_queue_push_tail (&priv->idle_queue, task);
    g_cond_signal (&priv->idle_cond);
    g_mutex_unlock (&priv->idle_mutex);
}

static GstMfxEncTask *
gst_mfx_enc_pop_idle_task (GstMfxEnc *self)
{
    GstMfxEncPrivate *priv = GST_MFX_ENC_GET_PRIVATE (self);
    GstMfxEncTask *task = NULL;

    g_mutex_lock (&priv->idle_mutex);
    while (priv->src_pad_push_status &&
                !g_queue_peek_head (&priv->idle_queue))
      g_cond_wait (&priv->idle_cond, &priv->idle_mutex);
    task = g_queue_pop_head (&priv->idle_queue);
    g_mutex_unlock (&priv->idle_mutex);

    return task;
}

static GstFlowReturn
gst_mfx_enc_sync_task(GstMfxEnc *self, gboolean send)
{
	GstMfxBase *parent = GST_MFX_BASE(self);
	GstMfxEncPrivate *priv = GST_MFX_ENC_GET_PRIVATE(self);
	GstFlowReturn ret = GST_FLOW_OK;
	GstMfxEncTask *task = NULL;
	mfxStatus s = MFX_ERR_NONE;

	/* Pop task from exec queue */
	task = gst_mfx_enc_pop_exec_task(self);
	if (G_UNLIKELY(NULL == task))
		return GST_FLOW_EOS;
	do {
		s = MFXVideoCORE_SyncOperation(parent->mfx_session,
			task->sp, G_MAXUINT32);
		/* The async operation is ready, push to src pad */
		if (MFX_ERR_NONE == s) {
			GstBuffer *buffer = NULL;
			GstMapInfo info;
			GstCaps *caps = NULL;
			GstQuery *query;
			GstBufferPool *pool;
			GstStructure *config;
			guint size, min, max;

			if (send) {
				/* find a pool for the negotiated caps now */
				query = gst_query_new_allocation(caps, TRUE);

				if (!gst_pad_peer_query(priv->src_pad, query)) {
					/* query failed, not a problem, we use the query defaults */
				}

				if (gst_query_get_n_allocation_pools(query) > 0) {
					/* we got configuration from our peer, parse them */
					gst_query_parse_nth_allocation_pool(query, 0, &pool, &size, &min, &max);
				}
				else {
					pool = NULL;
					size = 0;
					min = max = 0;
				}

				if (pool == NULL) {
					/* we did not get a pool, make one ourselves then */
					pool = gst_video_buffer_pool_new();
				}

				config = gst_buffer_pool_get_config(pool);
				gst_buffer_pool_config_add_option(config, GST_BUFFER_POOL_OPTION_VIDEO_META);
				gst_buffer_pool_config_set_params(config, caps, size, min, max);
				gst_buffer_pool_set_config(pool, config);

				/* and activate */
				gst_buffer_pool_set_active(pool, TRUE);

				ret = gst_buffer_pool_acquire_buffer(pool, &buffer, NULL);
				if (GST_FLOW_OK == ret && NULL != buffer) {
					//memcpy(GST_BUFFER_DATA(buffer), task->output.Data, task->output.DataLength);
					gst_buffer_map(buffer, &info, GST_MAP_READ);
					memcpy(info.data, task->output.Data, task->output.DataLength);
					gst_buffer_unmap(buffer, &info);
					
					if (G_LIKELY(0 == task->output.DataOffset))
						GST_BUFFER_OFFSET(buffer) = GST_BUFFER_OFFSET_NONE;
					else
						GST_BUFFER_OFFSET(buffer) = task->output.DataOffset;
					GST_BUFFER_TIMESTAMP(buffer) = task->output.TimeStamp;
					GST_BUFFER_DURATION(buffer) = task->duration;

					ret = gst_pad_push(priv->src_pad, buffer);
				}
			}

			/*if (send) {
				ret = gst_pad_alloc_buffer(priv->src_pad,
					GST_BUFFER_OFFSET_NONE,
					task->output.DataLength,
					priv->src_pad_caps,
					&buffer);
				if (GST_FLOW_OK == ret && NULL != buffer) {
					memcpy(GST_BUFFER_DATA(buffer),
						task->output.Data,
						task->output.DataLength);
					if (G_LIKELY(0 == task->output.DataOffset))
						GST_BUFFER_OFFSET(buffer) = GST_BUFFER_OFFSET_NONE;
					else
						GST_BUFFER_OFFSET(buffer) = task->output.DataOffset;
					GST_BUFFER_TIMESTAMP(buffer) = task->output.TimeStamp;
					GST_BUFFER_DURATION(buffer) = task->duration;

					ret = gst_pad_push(priv->src_pad, buffer);
				}
			}*/

			/* Push task to idle queue */
			gst_mfx_enc_push_idle_task(self, task);
		}
		else if (MFX_ERR_NONE > s) {
			/* Push task to idle queue */
			gst_mfx_enc_push_idle_task(self, task);
		}
	} while (MFX_ERR_NONE < s);

	return ret;
}

static void
gst_mfx_enc_flush_frames (GstMfxEnc *self, gboolean send)
{
    GstMfxEncPrivate *priv = GST_MFX_ENC_GET_PRIVATE (self);

    while (g_queue_peek_head (&priv->exec_queue))
      gst_mfx_enc_sync_task (self, send);
}

static void
gst_mfx_enc_set_property (GObject *obj, guint id,
            const GValue *value, GParamSpec *pspec)
{
    GstMfxEnc *self = GST_MFX_ENC (obj);
    GstMfxEncPrivate *priv = GST_MFX_ENC_GET_PRIVATE (self);

    switch (id) {
    case PROP_CODEC_ID:
        priv->codec_id = g_value_get_enum (value);
        break;
    case PROP_CODEC_PROFILE:
        priv->codec_profile = g_value_get_enum (value);
		break;
    case PROP_CODEC_LEVEL:
        priv->codec_level = g_value_get_enum (value);
		break;
    case PROP_NUM_THREAD:
        priv->num_thread = g_value_get_uint (value);
		break;
    case PROP_TARGET_USAGE:
        priv->target_usage = g_value_get_enum (value);
		break;
    case PROP_GOP_PIC_SIZE:
        priv->gop_pic_size = g_value_get_uint (value);
		break;
    case PROP_GOP_REF_DIST:
        priv->gop_ref_dist = g_value_get_uint (value);
		break;
    case PROP_GOP_OPT_FLAG:
        priv->gop_opt_flag = g_value_get_enum (value);
		break;
    case PROP_IDR_INTERVAL:
        priv->idr_interval = g_value_get_uint (value);
		break;
    case PROP_RATE_CTL_METHOD:
        priv->rate_ctl_method = g_value_get_enum (value);
		break;
    case PROP_INIT_DELAY:
        priv->init_delay = g_value_get_uint (value);
		break;
    case PROP_BITRATE:
        priv->bitrate = g_value_get_uint (value);
		break;
    case PROP_MAX_BITRATE:
        priv->max_bitrate = g_value_get_uint (value);
		break;
    case PROP_NUM_SLICE:
        priv->num_slice = g_value_get_uint (value);
		break;
    case PROP_NUM_REF_FRAME:
        priv->num_ref_frame = g_value_get_uint (value);
		break;
    case PROP_ENCODED_ORDER:
        priv->encoded_order = g_value_get_uint (value);
		break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, id, pspec);
        break;
    }
}

static void
gst_mfx_enc_get_property (GObject *obj, guint id,
            GValue *value, GParamSpec *pspec)
{
    GstMfxEnc *self = GST_MFX_ENC (obj);
    GstMfxEncPrivate *priv = GST_MFX_ENC_GET_PRIVATE (self);

    switch (id) {
    case PROP_CODEC_ID:
        g_value_set_enum (value, priv->codec_id);
        break;
    case PROP_CODEC_PROFILE:
        g_value_set_enum (value, priv->codec_profile);
		break;
    case PROP_CODEC_LEVEL:
        g_value_set_enum (value, priv->codec_level);
		break;
    case PROP_NUM_THREAD:
        g_value_set_uint (value, priv->num_thread);
		break;
    case PROP_TARGET_USAGE:
        g_value_set_enum (value, priv->target_usage);
		break;
    case PROP_GOP_PIC_SIZE:
        g_value_set_uint (value, priv->gop_pic_size);
		break;
    case PROP_GOP_REF_DIST:
        g_value_set_uint (value, priv->gop_ref_dist);
		break;
    case PROP_GOP_OPT_FLAG:
        g_value_set_enum (value, priv->gop_opt_flag);
		break;
    case PROP_IDR_INTERVAL:
        g_value_set_uint (value, priv->idr_interval);
		break;
    case PROP_RATE_CTL_METHOD:
        g_value_set_enum (value, priv->rate_ctl_method);
		break;
    case PROP_INIT_DELAY:
        g_value_set_uint (value, priv->init_delay);
		break;
    case PROP_BITRATE:
        g_value_set_uint (value, priv->bitrate);
		break;
    case PROP_MAX_BITRATE:
        g_value_set_uint (value, priv->max_bitrate);
		break;
    case PROP_NUM_SLICE:
        g_value_set_uint (value, priv->num_slice);
		break;
    case PROP_NUM_REF_FRAME:
        g_value_set_uint (value, priv->num_ref_frame);
		break;
    case PROP_ENCODED_ORDER:
        g_value_set_uint (value, priv->encoded_order);
		break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, id, pspec);
        break;
    }
}

static GstStateChangeReturn
gst_mfx_enc_change_state (GstElement *element,
            GstStateChange transition)
{
    GstMfxEnc *self = GST_MFX_ENC (element);
    GstMfxBase *parent = GST_MFX_BASE (self);
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

    if (GST_STATE_CHANGE_DIR_UPWARDS ==
            GST_STATE_CHANGE_DIR (transition)) {
        ret = GST_ELEMENT_CLASS (gst_mfx_enc_parent_class)->change_state (element, transition);
        if (GST_STATE_CHANGE_FAILURE == ret)
          goto out;
    }

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        gst_mfx_enc_flush_frames (self, FALSE);
        MFXVideoENCODE_Close (parent->mfx_session);
        break;
    case GST_STATE_CHANGE_READY_TO_NULL:
        break;
    default:
        break;
    }

    if (GST_STATE_CHANGE_DIR_DOWNWARDS ==
            GST_STATE_CHANGE_DIR (transition)) {
        ret = GST_ELEMENT_CLASS (gst_mfx_enc_parent_class)->change_state (element, transition);
        if (GST_STATE_CHANGE_FAILURE == ret)
          goto out;
    }

out:
    return ret;
}

static gboolean
gst_mfx_enc_setcaps (GstPad *pad, GstCaps *caps)
{
    GstMfxEnc *self = GST_MFX_ENC (GST_OBJECT_PARENT (pad));
    GstMfxEncPrivate *priv = GST_MFX_ENC_GET_PRIVATE (self);
    GstMfxBase *parent = GST_MFX_BASE (self);
    GstStructure *structure = NULL;
    mfxStatus s = MFX_ERR_NONE;
    mfxFrameAllocRequest req;
    gint width = 0, height = 0;
    gint numerator = 0, denominator = 0;
	
	gboolean ret;
	GstCaps *outcaps;

    if (!GST_CAPS_IS_SIMPLE (caps))
      goto fail;

    if (priv->task_pool)
      gst_mfx_enc_flush_frames (self, TRUE);

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

    memset (&priv->mfx_video_param, 0, sizeof (mfxVideoParam));
    g_object_get (self,
                "async-depth", &priv->mfx_video_param.AsyncDepth,
                "protected", &priv->mfx_video_param.Protected,
                "io-pattern", &priv->mfx_video_param.IOPattern,
                NULL);
    priv->mfx_video_param.mfx.CodecId = priv->codec_id;
    priv->mfx_video_param.mfx.CodecProfile = priv->codec_profile;
    priv->mfx_video_param.mfx.CodecLevel = priv->codec_level;
    priv->mfx_video_param.mfx.NumThread = priv->num_thread;
    priv->mfx_video_param.mfx.TargetUsage = priv->target_usage;
    priv->mfx_video_param.mfx.GopPicSize = priv->gop_pic_size;
    priv->mfx_video_param.mfx.GopRefDist = priv->gop_ref_dist;
    priv->mfx_video_param.mfx.GopOptFlag = priv->gop_opt_flag;
    priv->mfx_video_param.mfx.IdrInterval = priv->idr_interval;
    priv->mfx_video_param.mfx.RateControlMethod = priv->rate_ctl_method;
    priv->mfx_video_param.mfx.InitialDelayInKB = priv->init_delay;
    priv->mfx_video_param.mfx.TargetKbps = priv->bitrate;
    priv->mfx_video_param.mfx.MaxKbps = priv->max_bitrate;
    priv->mfx_video_param.mfx.NumSlice = priv->num_slice;
    priv->mfx_video_param.mfx.NumRefFrame = priv->num_ref_frame;
    priv->mfx_video_param.mfx.EncodedOrder = priv->encoded_order;
    priv->mfx_video_param.mfx.FrameInfo.Width = width;
    priv->mfx_video_param.mfx.FrameInfo.Height = height;
    priv->mfx_video_param.mfx.FrameInfo.FrameRateExtD = denominator;
    priv->mfx_video_param.mfx.FrameInfo.FrameRateExtN = numerator;
    priv->mfx_video_param.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    priv->mfx_video_param.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    priv->mfx_video_param.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    priv->mfx_video_param.mfx.FrameInfo.CropX = 0;
    priv->mfx_video_param.mfx.FrameInfo.CropY = 0;
    priv->mfx_video_param.mfx.FrameInfo.CropW = width;
    priv->mfx_video_param.mfx.FrameInfo.CropH = height;
    s = MFXVideoENCODE_Init (parent->mfx_session, &priv->mfx_video_param);
    if (MFX_ERR_NONE != s) {
        GST_ERROR ("MFXVideoENCODE_Init failed(%d)!", s);
        goto fail;
    }

    s = MFXVideoENCODE_GetVideoParam (parent->mfx_session, &priv->mfx_video_param);
    if (MFX_ERR_NONE != s) {
        GST_ERROR ("MFXVideoENCODE_GetVideoParam failed(%d)!", s);
        goto fail;
    }
    switch (priv->mfx_video_param.mfx.FrameInfo.FourCC) {
    case MFX_FOURCC_NV12:
        priv->fs_buf_len = width * height +
            (width>>1) * (height>>1) +
            (width>>1) * (height>>1);
        break;
    }
    priv->bs_buf_len = priv->mfx_video_param.mfx.BufferSizeInKB * 1024;

    s = MFXVideoENCODE_QueryIOSurf (parent->mfx_session,
                &priv->mfx_video_param, &req);
    if (MFX_ERR_NONE != s) {
        GST_ERROR ("MFXVideoENCODE_QueryIOSurf failed(%d)!", s);
        goto fail;
    }
    /* Free previous task pool */
    if (priv->task_pool) {
        guint i = 0;

        for (i=0; i<priv->task_pool_len; i++) {
            g_slice_free1 (priv->fs_buf_len,
                        priv->task_pool[i].input.Data.MemId);
            g_slice_free1 (priv->bs_buf_len,
                        priv->task_pool[i].output.Data);
        }
        g_slice_free1 (sizeof (GstMfxEncTask) * priv->task_pool_len,
                    priv->task_pool);

        while (g_queue_pop_head (&priv->exec_queue));
        while (g_queue_pop_head (&priv->idle_queue));
    }
    /* Alloc new task pool */
    priv->task_pool_len = req.NumFrameSuggested;
    priv->task_pool = g_slice_alloc0 (sizeof (GstMfxEncTask) * priv->task_pool_len);
    {
        guint i = 0;

        for (i=0; i<priv->task_pool_len; i++) {
            GstMfxEncTask *task = &priv->task_pool[i];

            /* Set input frame info */
            memcpy (&task->input.Info,
                        &priv->mfx_video_param.mfx.FrameInfo,
                        sizeof (mfxFrameInfo));
            /* Alloc buffer for input: mfxFrameSurface1 */
            task->input.Data.MemId = g_slice_alloc0 (priv->fs_buf_len);
            switch (priv->mfx_video_param.mfx.FrameInfo.FourCC) {
            case MFX_FOURCC_NV12:
                task->input.Data.Y = task->input.Data.MemId;
                task->input.Data.U = task->input.Data.Y + width * height;
                task->input.Data.V = task->input.Data.U + 1;
                task->input.Data.Pitch = width;
                break;
            }
            /* Alloc buffer for output: mfxBitstream */
            task->output.Data = g_slice_alloc0 (priv->bs_buf_len);
            task->output.MaxLength = priv->bs_buf_len;

            /* Push task to idle queue */
            gst_mfx_enc_push_idle_task (self, task);
        }
    }

    if (priv->src_pad_caps)
      gst_caps_unref (priv->src_pad_caps);
    /*structure = gst_structure_new ("video/x-h264",
                "width", G_TYPE_INT, width,
                "height", G_TYPE_INT, height,
                "framerate", GST_TYPE_FRACTION, numerator, denominator,
                "stream-format", G_TYPE_STRING, "avc",
                "alignment", G_TYPE_STRING, "au",
                NULL);*/

	outcaps = gst_structure_new("video/x-h264",
		"width", G_TYPE_INT, width,
		"height", G_TYPE_INT, height,
		"framerate", GST_TYPE_FRACTION, numerator, denominator,
		"stream-format", G_TYPE_STRING, "avc",
		"alignment", G_TYPE_STRING, "au",
		NULL);
    
	//priv->src_pad_caps = gst_caps_new_full (structure, NULL);

	ret = gst_pad_set_caps(priv->src_pad_caps, outcaps);

    return ret;

fail:

    return FALSE;
}

static gboolean
gst_mfx_enc_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    GstMfxEnc *self = GST_MFX_ENC (parent);
    GstMfxEncPrivate *priv = GST_MFX_ENC_GET_PRIVATE (self);
	gboolean ret;

    switch (GST_EVENT_TYPE (event)) {
	case GST_EVENT_CAPS:
	{
		GstCaps *caps;

		gst_event_parse_caps(event, &caps);
		ret = gst_mfx_enc_setcaps(self, caps);
		break;
	}
    case GST_EVENT_EOS:
        gst_mfx_enc_flush_frames (self, TRUE);
		ret = gst_pad_push_event(priv->src_pad, event);
        break;
    default:
		ret = gst_pad_event_default(pad, parent, event);
        break;
    }

    return ret;
}

static gboolean
gst_mfx_enc_decide_allocation_default(GstMfxEnc * encoder,
GstQuery * query)
{
	GstCaps *outcaps = NULL;
	GstBufferPool *pool = NULL;
	guint size, min, max;
	GstAllocator *allocator = NULL;
	GstAllocationParams params;
	GstStructure *config;
	gboolean update_pool, update_allocator;
	GstVideoInfo vinfo;

	gst_query_parse_allocation(query, &outcaps, NULL);
	gst_video_info_init(&vinfo);
	if (outcaps)
		gst_video_info_from_caps(&vinfo, outcaps);

	/* we got configuration from our peer or the decide_allocation method,
	* parse them */
	if (gst_query_get_n_allocation_params(query) > 0) {
		/* try the allocator */
		gst_query_parse_nth_allocation_param(query, 0, &allocator, &params);
		update_allocator = TRUE;
	}
	else {
		allocator = NULL;
		gst_allocation_params_init(&params);
		update_allocator = FALSE;
	}

	if (gst_query_get_n_allocation_pools(query) > 0) {
		gst_query_parse_nth_allocation_pool(query, 0, &pool, &size, &min, &max);
		size = MAX(size, vinfo.size);
		update_pool = TRUE;
	}
	else {
		pool = NULL;
		size = vinfo.size;
		min = max = 0;

		update_pool = FALSE;
	}

	if (pool == NULL) {
		/* no pool, we can make our own */
		GST_DEBUG_OBJECT(encoder, "no pool, making new pool");
		pool = gst_video_buffer_pool_new();
	}

	/* now configure */
	config = gst_buffer_pool_get_config(pool);
	gst_buffer_pool_config_set_params(config, outcaps, size, min, max);
	gst_buffer_pool_config_set_allocator(config, allocator, &params);

	if (!gst_buffer_pool_set_config(pool, config)) {
		config = gst_buffer_pool_get_config(pool);

		/* If change are not acceptable, fallback to generic pool */
		if (!gst_buffer_pool_config_validate_params(config, outcaps, size, min,
			max)) {
			GST_DEBUG_OBJECT(encoder, "unsuported pool, making new pool");

			gst_object_unref(pool);
			pool = gst_video_buffer_pool_new();
			gst_buffer_pool_config_set_params(config, outcaps, size, min, max);
			gst_buffer_pool_config_set_allocator(config, allocator, &params);
		}

		if (!gst_buffer_pool_set_config(pool, config))
			goto config_failed;
	}

	if (update_allocator)
		gst_query_set_nth_allocation_param(query, 0, allocator, &params);
	else
		gst_query_add_allocation_param(query, allocator, &params);
	if (allocator)
		gst_object_unref(allocator);

	if (update_pool)
		gst_query_set_nth_allocation_pool(query, 0, pool, size, min, max);
	else
		gst_query_add_allocation_pool(query, pool, size, min, max);

	if (pool)
		gst_object_unref(pool);

	return TRUE;

config_failed:
	if (allocator)
		gst_object_unref(allocator);
	if (pool)
		gst_object_unref(pool);
	GST_ELEMENT_ERROR(encoder, RESOURCE, SETTINGS,
		("Failed to configure the buffer pool"),
		("Configuration is most likely invalid, please report this issue."));
	return FALSE;
}

static GstFlowReturn
gst_mfx_enc_sink_pad_bufferalloc(GstPad *pad, guint64 offset,
guint size, GstCaps *caps, GstBuffer **buf)
{
	GstMfxEnc *self = GST_MFX_ENC(GST_OBJECT_PARENT(pad));
	GstMfxEncPrivate *priv = GST_MFX_ENC_GET_PRIVATE(self);
	GstMfxEncTask *task = NULL;

	if (G_UNLIKELY(GST_FLOW_OK != priv->src_pad_ret))
		return priv->src_pad_ret;

	/* No task pool, alloc a normal buffer */
	if (G_UNLIKELY(!priv->task_pool)) {
		*buf = gst_buffer_new_and_alloc(size);
		if (!*buf)
			return GST_FLOW_ERROR;
		GST_BUFFER_OFFSET(*buf) = offset;

		//gst_buffer_set_caps(*buf, caps);

		return GST_FLOW_OK;
	}

	if (G_UNLIKELY(size != priv->fs_buf_len))
		g_assert_not_reached();

	/* Pop task from idle queue */
	task = gst_mfx_enc_pop_idle_task(self);
	if (NULL == task)
		return GST_FLOW_ERROR;

	/**buf = gst_buffer_new();
	if (!*buf)
		return GST_FLOW_ERROR;
	GST_BUFFER_DATA(*buf) = task->input.Data.MemId;
	GST_BUFFER_SIZE(*buf) = priv->fs_buf_len;*/

	*buf = gst_buffer_new_wrapped(task->input.Data.MemId, priv->fs_buf_len);
	if (!*buf)
		return GST_FLOW_ERROR;

	GST_BUFFER_OFFSET(*buf) = GST_BUFFER_OFFSET_NONE;
	
	//gst_buffer_set_caps(*buf, caps);

	/* Save the task in task_curr */
	priv->task_curr = task;

	return GST_FLOW_OK;
}


static GstFlowReturn
gst_mfx_enc_chain(GstPad *pad, GstObject * parent, GstBuffer *buf)
{
	GstMfxEnc *self = GST_MFX_ENC(parent);
	GstMfxEncPrivate *priv = GST_MFX_ENC_GET_PRIVATE(self);
	GstMfxBase *mfxParent = GST_MFX_BASE(self);
	GstMfxEncTask *task = NULL;
	GstFlowReturn ret = GST_FLOW_OK;
	gboolean retry = TRUE, mcpy = FALSE;
	GstMapInfo info;

	gst_buffer_map(buf, &info, GST_MAP_READ);
	
	/* Guess the input buf is in task_curr */
	if (priv->task_curr && info.data == priv->task_curr->input.Data.MemId) {
		task = priv->task_curr;
		priv->task_curr = NULL;
	}
	else {
		//gint i = 0;
		/* Oh, is wrong! find buf's owner: task */
		for (guint i = 0; i<priv->task_pool_len; i++) {
			if (priv->task_pool[i].input.Data.MemId == info.data) {
				task = &priv->task_pool[i];
				break;
			}
		}
	}

	/* Not found in task pool, may be is first alloced buffer.
	* Get a idle task to handle it and set mcpy = TRUE.
	*/
	if (NULL == task) {
		/* Pop task from idle queue */
		task = gst_mfx_enc_pop_idle_task(self);
		if (NULL == task)
			goto fail;
		mcpy = TRUE;
	}

	/* Input: mfxFrameSurface1 */
	if (G_UNLIKELY(mcpy)) {
		if (priv->fs_buf_len != gst_buffer_get_size(buf))
			g_assert_not_reached();
		memcpy(task->input.Data.MemId, info.data, priv->fs_buf_len);
	}
	task->input.Data.TimeStamp = GST_BUFFER_TIMESTAMP(buf);

	/* Output: save duration */
	task->duration = GST_BUFFER_DURATION(buf);

	gst_buffer_unmap(buf, &info);

	/* Free input buffer: GstBuffer */
	gst_buffer_unref(buf);

	/* Commit the task to MFX Encoder */
	do {
		mfxStatus s = MFX_ERR_NONE;

		s = MFXVideoENCODE_EncodeFrameAsync(mfxParent->mfx_session, NULL,
			&task->input, &task->output, &task->sp);

		if (MFX_ERR_NONE < s && !task->sp) {
			if (MFX_WRN_DEVICE_BUSY == s)
				g_usleep(100);
			retry = TRUE;
		}
		else if (MFX_ERR_NONE < s && task->sp) {
			retry = FALSE;
		}
		else if (MFX_ERR_MORE_DATA == s) {
			retry = TRUE;
		}
		else if (MFX_ERR_NONE != s) {
			GST_ERROR("MFXVideoENCODE_EncodeFrameAsync failed(%d)!", s);
			ret = GST_FLOW_ERROR;
			goto fail;
		}
		else {
			retry = FALSE;
		}
	} while (retry);

	/* Push task to exec queue */
	gst_mfx_enc_push_exec_task(self, task);

	return ret;

fail:

	return ret;
}

static gboolean
gst_mfx_enc_src_activate_mode (GstPad *pad, GstObject *parent, GstPadMode mode, gboolean active)
{
    GstMfxEnc *self = GST_MFX_ENC (GST_OBJECT_PARENT (pad));
    GstMfxEncPrivate *priv = GST_MFX_ENC_GET_PRIVATE (self);
    gboolean ret = TRUE;

	switch (mode) {
	case GST_PAD_MODE_PUSH:
		if (active) {
			priv->src_pad_push_status = TRUE;
			ret = gst_pad_start_task(priv->src_pad,
				gst_mfx_enc_src_pad_task_handler, self, NULL);
		}
		else {
			/* Send a quit signal to task thread */
			g_mutex_lock(&priv->exec_mutex);
			priv->src_pad_push_status = FALSE;
			g_cond_signal(&priv->exec_cond);
			g_mutex_unlock(&priv->exec_mutex);
			ret = gst_pad_stop_task(priv->src_pad);
		}
	case GST_PAD_MODE_PULL:
	default:
		ret = FALSE;
		break;
	}

    return ret;
}

static void
gst_mfx_enc_src_pad_task_handler (gpointer data)
{
    GstMfxEnc *self = GST_MFX_ENC (data);
    GstMfxEncPrivate *priv = GST_MFX_ENC_GET_PRIVATE (self);

    priv->src_pad_ret = gst_mfx_enc_sync_task (self, TRUE);
    if (G_UNLIKELY (GST_FLOW_OK != priv->src_pad_ret))
      gst_pad_pause_task (priv->src_pad);
}

