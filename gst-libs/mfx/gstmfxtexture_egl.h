#ifndef GST_MFX_TEXTURE_EGL_H
#define GST_MFX_TEXTURE_EGL_H

#include "gstmfxtexture.h"
#include "gstmfxtexture_priv.h"
#include "gstmfxutils_egl.h"

G_BEGIN_DECLS

#define GST_MFX_TEXTURE_EGL(texture) \
	((GstMfxTextureEGL *) (texture))

typedef struct _GstMfxTextureEGL GstMfxTextureEGL;
typedef struct _GstMfxTextureEGLClass GstMfxTextureEGLClass;

/**
* GstMfxTextureEGL:
*
* Base object for EGL texture wrapper.
*/
struct _GstMfxTextureEGL
{
	/*< private >*/
	GstMfxTexture parent_instance;

	EglContext *egl_context;
	EGLImageKHR egl_images[3];
	GLuint textures[3];
	int num_textures;
};

/**
* GstMfxTextureEGLClass:
*
* Base class for EGL texture wrapper.
*/
struct _GstMfxTextureEGLClass
{
	/*< private >*/
	GstMfxTextureClass parent_class;
};

GstMfxTexture *
gst_mfx_texture_egl_new(GstMfxDisplay * display, guint target,
	guint format, guint width, guint height);

GstMfxTexture *
gst_mfx_texture_egl_new_wrapped(GstMfxDisplay * display, guint id,
	guint target, guint format, guint width, guint height);

G_END_DECLS

#endif /* GST_MFX_TEXTURE_EGL_H */
