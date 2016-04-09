#include "video-format.h"

typedef struct _GstMfxFormatMap GstMfxFormatMap;

struct _GstMfxFormatMap
{
    GstVideoFormat  format;
    mfxU32          mfx_fourcc;
    mfxU16          mfx_chroma;
    guint           va_fourcc;
    guint           va_format;
};

GstMfxFormatMap format_map[] = {
    { GST_VIDEO_FORMAT_NV12, MFX_FOURCC_NV12, MFX_CHROMAFORMAT_YUV420,
        VA_FOURCC_NV12, VA_RT_FORMAT_YUV420 },
    { GST_VIDEO_FORMAT_YV12, MFX_FOURCC_YV12, MFX_CHROMAFORMAT_YUV420,
        VA_FOURCC_YV12, VA_RT_FORMAT_YUV420 },
    { GST_VIDEO_FORMAT_YUY2, MFX_FOURCC_YUY2, MFX_CHROMAFORMAT_YUV422,
        VA_FOURCC_YUY2, VA_RT_FORMAT_YUV422 },
    { GST_VIDEO_FORMAT_UYVY, MFX_FOURCC_UYVY, MFX_CHROMAFORMAT_YUV422,
        VA_FOURCC_UYVY, VA_RT_FORMAT_YUV422 },
    { GST_VIDEO_FORMAT_BGRA, MFX_FOURCC_RGB4, MFX_CHROMAFORMAT_YUV444,
        VA_FOURCC_BGRA, VA_RT_FORMAT_RGB32  },
    {0,}
};

GstVideoFormat
gst_video_format_from_mfx_fourcc(mfxU32 fourcc)
{
    GstMfxFormatMap *m;
    for(m = format_map; m->format; m++)
    {
        if(fourcc == m->mfx_fourcc)
            return m->format;
    }
    return GST_VIDEO_FORMAT_UNKNOWN;
}

mfxU32
gst_video_format_to_mfx_fourcc(GstVideoFormat format)
{
    GstMfxFormatMap *m;
    for(m = format_map; m->format; m++)
    {
        if(format == m->format)
            return m->mfx_fourcc;
    }
    return 0;
}

GstVideoFormat
gst_video_format_from_va_fourcc(guint fourcc)
{
    GstMfxFormatMap *m;
    for(m = format_map; m->format; m++)
    {
         if(fourcc == m->va_fourcc)
             return m->format;
    }
    return GST_VIDEO_FORMAT_UNKNOWN;
}

guint
gst_video_format_to_va_fourcc(GstVideoFormat format)
{
    GstMfxFormatMap *m;
    for(m = format_map; m->format; m++)
    {
        if(format == m->format)
            return m->va_fourcc;
    }
    return 0;
}

mfxU32
gst_mfx_video_format_from_va_fourcc(guint fourcc)
{
    GstMfxFormatMap *m;
    for(m = format_map; m->format; m++)
    {
        if(fourcc == m->va_fourcc)
            return m->mfx_fourcc;
    }
    return 0;
}

guint
gst_mfx_video_format_to_va_fourcc(mfxU32 fourcc)
{
    GstMfxFormatMap *m;
    for(m = format_map; m->format; m++)
    {
        if(fourcc == m->mfx_fourcc)
            return m->va_fourcc;
    }
    return 0;
}

guint
gst_mfx_video_format_to_va_format(mfxU32 fourcc)
{
    GstMfxFormatMap *m;
    for(m = format_map; m->format; m++)
    {
        if(fourcc == m->mfx_fourcc)
            return m->va_format;
    }
    return 0;
}

guint16
gst_mfx_chroma_type_from_video_format(GstVideoFormat format)
{
    GstMfxFormatMap *m;
    for(m = format_map; m->format; m++)
    {
        if(format == m->format)
            return m->mfx_chroma;
    }
    return 0;
}
