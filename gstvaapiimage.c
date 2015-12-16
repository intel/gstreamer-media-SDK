#include "gstvaapiimage.h"
#include "gstvaapiimage_priv.h"


static gboolean
_gst_vaapi_image_map(GstVaapiImage *image);

static gboolean
_gst_vaapi_image_unmap(GstVaapiImage *image);

static gboolean
_gst_vaapi_image_set_image(GstVaapiImage *image, const VAImage *va_image);

static void
gst_vaapi_image_finalize(GstVaapiImage *image)
{
    VAStatus status;

    _gst_vaapi_image_unmap(image);

    if (image->image_id != VA_INVALID_ID) {
        status = vaDestroyImage(image->display, image->image_id);
        if (!vaapi_check_status(status, "vaDestroyImage()"))
            g_warning("failed to destroy image");
        image->image_id = VA_INVALID_ID;
    }
}

static void
gst_vaapi_image_init(GstVaapiImage *image, VADisplay display)
{
    image->display = display;
    image->image_id = VA_INVALID_ID;
    image->image.image_id = VA_INVALID_ID;
    image->image.buf = VA_INVALID_ID;
}

static inline const GstMfxMiniObjectClass *
gst_vaapi_image_class(void)
{
	static const GstMfxMiniObjectClass GstVaapiImageClass = {
		sizeof(GstVaapiImage),
		(GDestroyNotify)gst_vaapi_image_finalize
	};
	return &GstVaapiImageClass;
}

static gboolean
gst_vaapi_image_create(GstVaapiImage *image,
    guint width, guint height)
{
    VAStatus status;
    VAImageFormat va_format = {
        .fourcc         = VA_FOURCC_NV12,
        .byte_order     = VA_LSB_FIRST,
        .bits_per_pixel = 8,
        .depth          = 8,
    };

    status = vaCreateImage(
        image->display,
        &va_format,
        width,
        height,
        &image->image
    );

    if (status != VA_STATUS_SUCCESS)
        return FALSE;

    image->internal_format = GST_VIDEO_FORMAT_NV12;
    image->width = width;
    image->height = height;
    image->image_id = image->image.image_id;

    return TRUE;
}

/**
 * gst_vaapi_image_new:
 * @display: a #GstVaapiDisplay
 * @format: a #GstVideoFormat
 * @width: the requested image width
 * @height: the requested image height
 *
 * Creates a new #GstVaapiImage with the specified format and
 * dimensions.
 *
 * Return value: the newly allocated #GstVaapiImage object
 */
GstVaapiImage *
gst_vaapi_image_new(
    VADisplay           display,
    guint               width,
    guint               height
)
{
    GstVaapiImage *image;

    g_return_val_if_fail(width > 0, NULL);
    g_return_val_if_fail(height > 0, NULL);

    image = gst_mfx_mini_object_new0(gst_vaapi_image_class());

    if (!image)
        return NULL;

    gst_vaapi_image_init(image, display);
    if (!gst_vaapi_image_create(image, width, height))
        goto error;
    return image;

error:
    gst_mfx_mini_object_unref(image);
    return NULL;
}

/**
 * gst_vaapi_image_new_with_image:
 * @display: a #GstVaapiDisplay
 * @va_image: a VA image
 *
 * Creates a new #GstVaapiImage from a foreign VA image. The image
 * format and dimensions will be extracted from @va_image. This
 * function is mainly used by gst_vaapi_surface_derive_image() to bind
 * a VA image to a #GstVaapiImage object.
 *
 * Return value: the newly allocated #GstVaapiImage object
 */
GstVaapiImage *
gst_vaapi_image_new_with_image(VADisplay display, VAImage *va_image)
{
    GstVaapiImage *image;

    g_return_val_if_fail(va_image, NULL);
    g_return_val_if_fail(va_image->image_id != VA_INVALID_ID, NULL);
    g_return_val_if_fail(va_image->buf != VA_INVALID_ID, NULL);

    image = gst_mfx_mini_object_new0(gst_vaapi_image_class());
    if (!image)
        return NULL;

    gst_vaapi_image_init(image, display);
    if (!_gst_vaapi_image_set_image(image, va_image))
        goto error;
    return image;

error:
    gst_mfx_mini_object_unref(image);
    return NULL;
}

/**
 * gst_vaapi_image_get_id:
 * @image: a #GstVaapiImage
 *
 * Returns the underlying VAImageID of the @image.
 *
 * Return value: the underlying VA image id
 */
VAImageID
gst_vaapi_image_get_id(GstVaapiImage *image)
{
    g_return_val_if_fail(image != NULL, VA_INVALID_ID);

    return image->image_id;
}

/**
 * gst_vaapi_image_get_image:
 * @image: a #GstVaapiImage
 * @va_image: a VA image
 *
 * Fills @va_image with the VA image used internally.
 *
 * Return value: %TRUE on success
 */
gboolean
gst_vaapi_image_get_image(GstVaapiImage *image, VAImage *va_image)
{
    g_return_val_if_fail(image != NULL, FALSE);

    if (va_image)
        *va_image = image->image;

    return TRUE;
}

/*
 * _gst_vaapi_image_set_image:
 * @image: a #GstVaapiImage
 * @va_image: a VA image
 *
 * Initializes #GstVaapiImage with a foreign VA image. This function
 * will try to "linearize" the VA image. i.e. making sure that the VA
 * image offsets into the data buffer are in increasing order with the
 * number of planes available in the image.
 *
 * This is an internal function used by gst_vaapi_image_new_with_image().
 *
 * Return value: %TRUE on success
 */
gboolean
_gst_vaapi_image_set_image(GstVaapiImage *image, const VAImage *va_image)
{
    image->internal_format = GST_VIDEO_FORMAT_NV12;
    image->image           = *va_image;
    image->width           = va_image->width;
    image->height          = va_image->height;

    image->image_id = va_image->image_id;

    return TRUE;
}

/**
 * gst_vaapi_image_get_format:
 * @image: a #GstVaapiImage
 *
 * Returns the #GstVideoFormat the @image was created with.
 *
 * Return value: the #GstVideoFormat
 */
GstVideoFormat
gst_vaapi_image_get_format(GstVaapiImage *image)
{
    g_return_val_if_fail(image != NULL, 0);

    return image->internal_format;
}

/**
 * gst_vaapi_image_get_width:
 * @image: a #GstVaapiImage
 *
 * Returns the @image width.
 *
 * Return value: the image width, in pixels
 */
guint
gst_vaapi_image_get_width(GstVaapiImage *image)
{
    g_return_val_if_fail(image != NULL, 0);

    return image->width;
}

/**
 * gst_vaapi_image_get_height:
 * @image: a #GstVaapiImage
 *
 * Returns the @image height.
 *
 * Return value: the image height, in pixels.
 */
guint
gst_vaapi_image_get_height(GstVaapiImage *image)
{
    g_return_val_if_fail(image != NULL, 0);

    return image->height;
}

/**
 * gst_vaapi_image_get_size:
 * @image: a #GstVaapiImage
 * @pwidth: return location for the width, or %NULL
 * @pheight: return location for the height, or %NULL
 *
 * Retrieves the dimensions of a #GstVaapiImage.
 */
void
gst_vaapi_image_get_size(GstVaapiImage *image, guint *pwidth, guint *pheight)
{
    g_return_if_fail(image != NULL);

    if (pwidth)
        *pwidth = image->width;

    if (pheight)
        *pheight = image->height;
}

/**
 * gst_vaapi_image_is_mapped:
 * @image: a #GstVaapiImage
 *
 * Checks whether the @image is currently mapped or not.
 *
 * Return value: %TRUE if the @image is mapped
 */
static inline gboolean
_gst_vaapi_image_is_mapped(GstVaapiImage *image)
{
    return image->image_data != NULL;
}

gboolean
gst_vaapi_image_is_mapped(GstVaapiImage *image)
{
    g_return_val_if_fail(image != NULL, FALSE);

    return _gst_vaapi_image_is_mapped(image);
}

/**
 * gst_vaapi_image_map:
 * @image: a #GstVaapiImage
 *
 * Maps the image data buffer. The actual pixels are returned by the
 * gst_vaapi_image_get_plane() function.
 *
 * Return value: %TRUE on success
 */
gboolean
gst_vaapi_image_map(GstVaapiImage *image)
{
    g_return_val_if_fail(image != NULL, FALSE);

    return _gst_vaapi_image_map(image);
}

gboolean
_gst_vaapi_image_map(GstVaapiImage *image)
{
    VAStatus status;
    guint i;

    if (_gst_vaapi_image_is_mapped(image))
        goto map_success;

    if (!image->display)
        return FALSE;

    status = vaMapBuffer(
        image->display,
        image->image.buf,
        (void **)&image->image_data
    );

    if (!vaapi_check_status(status, "vaMapBuffer()"))
        return FALSE;

map_success:
    return TRUE;
}

/**
 * gst_vaapi_image_unmap:
 * @image: a #GstVaapiImage
 *
 * Unmaps the image data buffer. Pointers to pixels returned by
 * gst_vaapi_image_get_plane() are then no longer valid.
 *
 * Return value: %TRUE on success
 */
gboolean
gst_vaapi_image_unmap(GstVaapiImage *image)
{
    g_return_val_if_fail(image != NULL, FALSE);

    return _gst_vaapi_image_unmap(image);
}

gboolean
_gst_vaapi_image_unmap(GstVaapiImage *image)
{
    VAStatus status;

    if (!_gst_vaapi_image_is_mapped(image))
        return TRUE;

    if (!image->display)
        return FALSE;

    status = vaUnmapBuffer(
        image->display,
        image->image.buf
    );

    if (!vaapi_check_status(status, "vaUnmapBuffer()"))
        return FALSE;

    image->image_data = NULL;
    return TRUE;
}

/**
 * gst_vaapi_image_get_plane_count:
 * @image: a #GstVaapiImage
 *
 * Retrieves the number of planes available in the @image. The @image
 * must be mapped for this function to work properly.
 *
 * Return value: the number of planes available in the @image
 */
guint
gst_vaapi_image_get_plane_count(GstVaapiImage *image)
{
    g_return_val_if_fail(image != NULL, 0);
    g_return_val_if_fail(_gst_vaapi_image_is_mapped(image), 0);

    return image->image.num_planes;
}

/**
 * gst_vaapi_image_get_plane:
 * @image: a #GstVaapiImage
 * @plane: the requested plane number
 *
 * Retrieves the pixels data to the specified @plane. The @image must
 * be mapped for this function to work properly.
 *
 * Return value: the pixels data of the specified @plane
 */
guchar *
gst_vaapi_image_get_plane(GstVaapiImage *image, guint plane)
{
    g_return_val_if_fail(image != NULL, NULL);
    g_return_val_if_fail(_gst_vaapi_image_is_mapped(image), NULL);
    g_return_val_if_fail(plane < image->image.num_planes, NULL);

    return image->image_data + image->image.offsets[plane];
}

/**
 * gst_vaapi_image_get_pitch:
 * @image: a #GstVaapiImage
 * @plane: the requested plane number
 *
 * Retrieves the line size (stride) of the specified @plane. The
 * @image must be mapped for this function to work properly.
 *
 * Return value: the line size (stride) of the specified plane
 */
guint
gst_vaapi_image_get_pitch(GstVaapiImage *image, guint plane)
{
    g_return_val_if_fail(image != NULL, 0);
    g_return_val_if_fail(_gst_vaapi_image_is_mapped(image), 0);
    g_return_val_if_fail(plane < image->image.num_planes, 0);

    return image->image.pitches[plane];
}

/**
 * gst_vaapi_image_get_data_size:
 * @image: a #GstVaapiImage
 *
 * Retrieves the underlying image data size. This function could be
 * used to determine whether the image has a compatible layout with
 * another image structure.
 *
 * Return value: the whole image data size of the @image
 */
guint
gst_vaapi_image_get_data_size(GstVaapiImage *image)
{
    g_return_val_if_fail(image != NULL, 0);

    return image->image.data_size;
}
