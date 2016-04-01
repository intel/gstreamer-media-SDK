#include "video-format.h"

typedef struct _GstMfxFormatMap GstMfxFormatMap;

struct _GstMfxFormatMap
{
    GstVideoFormat  format;
    mfxU32          mfx_fourcc;
    guint           va_fourcc;
};

GstMfxFormatMap format_map[] = {
    {GST_VIDEO_FORMAT_NV12, MFX_FOURCC_NV12, VA_FOURCC_NV12},
    {GST_VIDEO_FORMAT_YV12, MFX_FOURCC_YV12, VA_FOURCC_YV12},
    {GST_VIDEO_FORMAT_YUY2, MFX_FOURCC_YUY2, VA_FOURCC_YUY2},
    {GST_VIDEO_FORMAT_UYVY, MFX_FOURCC_UYVY, VA_FOURCC_UYVY},
    {GST_VIDEO_FORMAT_RGBA, MFX_FOURCC_RGB4, VA_FOURCC_RGBA},
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
