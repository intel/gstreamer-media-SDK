#include "sysdeps.h"
#include "gstmfxwindow_egl.h"
#include "gstmfxwindow_priv.h"
#include "gstmfxtexture_egl.h"
#include "gstmfxtexture_priv.h"
#include "gstmfxdisplay_egl_priv.h"

#define GST_MFX_WINDOW_EGL(obj) \
	((GstMfxWindowEGL *)(obj))

#define GST_MFX_WINDOW_EGL_CLASS(klass) \
	((GstMfxWindowEGLClass *)(klass))

#define GST_MFX_WINDOW_EGL_GET_CLASS(obj) \
	GST_MFX_WINDOW_EGL_CLASS (GST_MFX_WINDOW_GET_CLASS (obj))

typedef struct _GstMfxWindowEGL GstMfxWindowEGL;
typedef struct _GstMfxWindowEGLClass GstMfxWindowEGLClass;

enum
{
	RENDER_PROGRAM_VAR_PROJ = 0,
	RENDER_PROGRAM_VAR_TEX0,
	RENDER_PROGRAM_VAR_TEX1,
	RENDER_PROGRAM_VAR_TEX2,
};

struct _GstMfxWindowEGL
{
	GstMfxWindow parent_instance;

	GstMfxWindow *window;
	GstMfxTexture *texture;
	EglWindow *egl_window;
	EglVTable *egl_vtable;
	EglProgram *render_program;
	gfloat render_projection[16];
};

struct _GstMfxWindowEGLClass
{
	GstMfxWindowClass parent_class;
};

typedef struct
{
	GstMfxWindowEGL *window;
	guint width;
	guint height;
	EglContext *egl_context;
	gboolean success;             /* result */
} CreateObjectsArgs;

typedef struct
{
	GstMfxWindowEGL *window;
	guint width;
	guint height;
	gboolean success;             /* result */
} ResizeWindowArgs;

typedef struct
{
	GstMfxWindowEGL *window;
	GstMfxSurface *surface;
	const GstMfxRectangle *src_rect;
	const GstMfxRectangle *dst_rect;
	gboolean success;             /* result */
} UploadSurfaceArgs;

static const gchar *vert_shader_text =
	"#ifdef GL_ES\n"
	"precision mediump float;\n"
	"#endif\n"
	"\n"
	"uniform mat4 proj;\n"
	"\n"
	"attribute vec2 position;\n"
	"attribute vec2 texcoord;\n"
	"varying vec2 v_texcoord;\n"
	"\n"
	"void main () {\n"
	"  gl_Position = proj * vec4 (position, 0.0, 1.0);\n"
	"  v_texcoord  = texcoord;\n"
	"}\n";

static const gchar *frag_shader_text_rgba =
	"#ifdef GL_ES\n"
	"precision mediump float;\n"
	"#endif\n"
	"\n"
	"uniform sampler2D tex0;\n"
	"\n"
	"varying vec2 v_texcoord;\n"
	"\n"
	"void main () {\n"
	"  gl_FragColor = texture2D (tex0, v_texcoord);\n"
	"}\n";

/*
 * For 8-bit per sample, and RGB limited range [16, 235]:
 *
 *             219                219                       219
 *   Y =  16 + --- * Kr     * R + --- * (1 - Kr - Kb) * G + --- * Kb     * B
 *             255                255                       255
 *
 *             112     Kr         112   1 - (Kr + Kb)       112
 *   U = 128 - --- * ------ * R - --- * ------------- * G + ---          * B
 *             255   1 - Kb       255      1 - Kb           255
 *
 *             112                112   1 - (Kr + Kb)       112     Kb
 *   V = 128 + ---          * R - --- * ------------- * G - --- * ------ * B
 *             255                255      1 - Kr           255   1 - Kr
 *
 * Constants for ITU-R BT.601 (SDTV):
 *   Kb = 0.114
 *   Kr = 0.299
 *
 * Constants for ITU-R BT.709 (HDTV):
 *   Kb = 0.0722
 *   Kr = 0.2126
 *
 * Constants for SMPTE 240M:
 *   Kb = 0.087
 *   Kr = 0.212
 *
 * Matrix generation with xcas:
 *   inverse([
 *   [  Kr         ,  1-(Kr+Kb)          ,  Kb        ]*219/255,
 *   [ -Kr/(1-Kb)  , -(1-(Kr+Kb))/(1-Kb) ,  1         ]*112/255,
 *   [  1          , -(1-(Kr+Kb))/(1-Kr) , -Kb/(1-Kr) ]*112/255])
 *
 * As a reminder:
 * - Kb + Kr + Kg = 1.0
 * - Y range is [0.0, 1.0], U/V range is [-1.0, 1.0]
 */
#define YUV2RGB_COLOR_BT601_LIMITED                     \
    "const vec3 yuv2rgb_ofs = vec3(0.0625, 0.5, 0.5);"  \
    "const mat3 yuv2rgb_mat = "                         \
    "    mat3(1.16438356,  0         ,  1.61651785, "   \
    "         1.16438356, -0.38584641, -0.78656070, "   \
    "         1.16438356,  2.01723214, 0          );\n"

#define YUV2RGB_COLOR_BT709_LIMITED                     \
    "const vec3 yuv2rgb_ofs = vec3(0.0625, 0.5, 0.5);"  \
    "const mat3 yuv2rgb_mat = "                         \
    "    mat3(1.16438356,  0         ,  1.79274107, "   \
    "         1.16438356, -0.21324861, -0.53290932, "   \
    "         1.16438356,  2.11240178, 0          );\n"

#define YUV2RGB_COLOR_SMPTE240M_LIMITED                 \
    "const vec3 yuv2rgb_ofs = vec3(0.0625, 0.5, 0.5);"  \
    "const mat3 yuv2rgb_mat = "                         \
    "    mat3(1.16438356,  0         ,  1.79410714, "   \
    "         1.16438356, -0.25798483, -0.54258304, "   \
    "         1.16438356,  2.07870535,  0         );\n"

#define YUV2RGB_COLOR(CONV)                             \
    G_PASTE(YUV2RGB_COLOR_,CONV)                   \
    "vec3 rgb = (yuv - yuv2rgb_ofs) * yuv2rgb_mat;\n"   \
    "gl_FragColor = vec4(rgb, 1);\n"

static const char *frag_shader_text_nv12 =
    "#ifdef GL_ES\n"
    "#extension GL_OES_EGL_image_external : require\n"
    "precision mediump float;\n"
    "uniform samplerExternalOES tex0;\n"
    "uniform samplerExternalOES tex1;\n"
    "#else\n"
    "uniform sampler2D tex0;\n"
    "uniform sampler2D tex1;\n"
    "#endif\n"
    "\n"
    "varying vec2 v_texcoord;\n"
    "\n"
    "void main() {\n"
    "    vec4 p_y  = texture2D(tex0, v_texcoord);\n"
    "    vec4 p_uv = texture2D(tex1, v_texcoord);\n"
    "    vec3 yuv  = vec3(p_y.r, p_uv.r, p_uv.g);\n"
    YUV2RGB_COLOR(BT709_LIMITED)
    "}\n";

static gboolean
ensure_texture(GstMfxWindowEGL * window, guint width, guint height)
{
	GstMfxTexture *texture;
	GstMfxDisplay *display = GST_MFX_OBJECT_DISPLAY(window);

	/*if (window->texture &&
		GST_MFX_TEXTURE_WIDTH(window->texture) == width &&
		GST_MFX_TEXTURE_HEIGHT(window->texture) == height)
		return TRUE;*/

	texture = gst_mfx_texture_egl_new(display,
		GST_MFX_DISPLAY_EGL(display)->gles_version == 0 ?
        GL_TEXTURE_2D : GL_TEXTURE_EXTERNAL_OES,
        GL_RGBA, width, height);

	gst_mfx_texture_replace(&window->texture, texture);
	gst_mfx_texture_replace(&texture, NULL);

	return window->texture != NULL;
}

static gboolean
ensure_shaders(GstMfxWindowEGL * window)
{
	EglVTable *const vtable = window->egl_vtable;
	EglProgram *program;
	GLuint prog_id;

	g_return_val_if_fail(window->texture != NULL, FALSE);
	g_return_val_if_fail(GST_MFX_TEXTURE_FORMAT(window->texture) == GL_RGBA,
		FALSE);

	if (window->render_program)
		return TRUE;

	//program = egl_program_new(window->egl_window->context,
		//frag_shader_text_rgba, vert_shader_text);
    program = egl_program_new(window->egl_window->context,
		frag_shader_text_nv12, vert_shader_text);
	if (!program)
		return FALSE;

	prog_id = program->base.handle.u;

	vtable->glUseProgram(prog_id);
	program->uniforms[RENDER_PROGRAM_VAR_PROJ] =
		vtable->glGetUniformLocation(prog_id, "proj");
	program->uniforms[RENDER_PROGRAM_VAR_TEX0] =
		vtable->glGetUniformLocation(prog_id, "tex0");
	program->uniforms[RENDER_PROGRAM_VAR_TEX1] =
		vtable->glGetUniformLocation(prog_id, "tex1");
	program->uniforms[RENDER_PROGRAM_VAR_TEX2] =
		vtable->glGetUniformLocation(prog_id, "tex2");
	vtable->glUseProgram(0);

	egl_matrix_set_identity(window->render_projection);

	egl_object_replace(&window->render_program, program);
	egl_object_replace(&program, NULL);
	return TRUE;
}

static gboolean
do_create_objects_unlocked(GstMfxWindowEGL * window, guint width,
	guint height, EglContext * egl_context)
{
	EglWindow *egl_window;
	EglVTable *egl_vtable;

    egl_window = egl_window_new(egl_context,
		GSIZE_TO_POINTER(GST_MFX_OBJECT_ID(window->window)));
	if (!egl_window)
		return FALSE;
	window->egl_window = egl_window;

	egl_vtable = egl_context_get_vtable(egl_window->context, TRUE);
	if (!egl_vtable)
		return FALSE;
	window->egl_vtable = egl_object_ref(egl_vtable);
	return TRUE;
}

static void
do_create_objects(CreateObjectsArgs * args)
{
	GstMfxWindowEGL *const window = args->window;
	EglContextState old_cs;

	args->success = FALSE;

	GST_MFX_OBJECT_LOCK_DISPLAY(window);
	if (egl_context_set_current(args->egl_context, TRUE, &old_cs)) {
		args->success = do_create_objects_unlocked(window, args->width,
			args->height, args->egl_context);
		egl_context_set_current(args->egl_context, FALSE, &old_cs);
	}
	GST_MFX_OBJECT_UNLOCK_DISPLAY(window);
}

static gboolean
gst_mfx_window_egl_create(GstMfxWindowEGL * window,
	guint * width, guint * height)
{
	GstMfxDisplayEGL *const display =
		GST_MFX_DISPLAY_EGL(GST_MFX_OBJECT_DISPLAY(window));
	const GstMfxDisplayClass *const native_dpy_class =
		GST_MFX_DISPLAY_GET_CLASS(display->display);
	CreateObjectsArgs args;

	g_return_val_if_fail(native_dpy_class != NULL, FALSE);

	window->window = native_dpy_class->create_window(GST_MFX_DISPLAY(display->display),
		*width, *height);
	if (!window->window)
		return FALSE;

	gst_mfx_window_get_size(window->window, width, height);

	args.window = window;
	args.width = *width;
	args.height = *height;
	args.egl_context = GST_MFX_DISPLAY_EGL_CONTEXT(display);
	return egl_context_run(args.egl_context,
		(EglContextRunFunc)do_create_objects, &args) && args.success;
}

static void
do_destroy_objects_unlocked(GstMfxWindowEGL * window)
{
	egl_object_replace(&window->render_program, NULL);
	egl_object_replace(&window->egl_vtable, NULL);
	egl_object_replace(&window->egl_window, NULL);
}

static void
do_destroy_objects(GstMfxWindowEGL * window)
{
	EglContext *const egl_context =
		GST_MFX_DISPLAY_EGL_CONTEXT(GST_MFX_OBJECT_DISPLAY(window));
	EglContextState old_cs;

	if (!window->egl_window)
		return;

	GST_MFX_OBJECT_LOCK_DISPLAY(window);
	if (egl_context_set_current(egl_context, TRUE, &old_cs)) {
		do_destroy_objects_unlocked(window);
		egl_context_set_current(egl_context, FALSE, &old_cs);
	}
	GST_MFX_OBJECT_UNLOCK_DISPLAY(window);
}

static void
gst_mfx_window_egl_destroy(GstMfxWindowEGL * window)
{
	egl_context_run(window->egl_window->context,
		(EglContextRunFunc)do_destroy_objects, window);
	gst_mfx_window_replace(&window->window, NULL);
	gst_mfx_texture_replace(&window->texture, NULL);
}

static gboolean
gst_mfx_window_egl_show(GstMfxWindowEGL * window)
{
	const GstMfxWindowClass *const klass =
		GST_MFX_WINDOW_GET_CLASS(window->window);

	g_return_val_if_fail(klass->show, FALSE);

	return klass->show(window->window);
}

static gboolean
gst_mfx_window_egl_hide(GstMfxWindowEGL * window)
{
	const GstMfxWindowClass *const klass =
		GST_MFX_WINDOW_GET_CLASS(window->window);

	g_return_val_if_fail(klass->hide, FALSE);

	return klass->hide(window->window);
}

static gboolean
gst_mfx_window_egl_get_geometry(GstMfxWindowEGL * window,
	gint * x_ptr, gint * y_ptr, guint * width_ptr, guint * height_ptr)
{
	const GstMfxWindowClass *const klass =
		GST_MFX_WINDOW_GET_CLASS(window->window);

	return klass->get_geometry ? klass->get_geometry(window->window,
		x_ptr, y_ptr, width_ptr, height_ptr) : FALSE;
}

static gboolean
gst_mfx_window_egl_set_fullscreen(GstMfxWindowEGL * window,
	gboolean fullscreen)
{
	const GstMfxWindowClass *const klass =
		GST_MFX_WINDOW_GET_CLASS(window->window);

	return klass->set_fullscreen ? klass->set_fullscreen(window->window,
		fullscreen) : FALSE;
}

static gboolean
do_resize_window_unlocked(GstMfxWindowEGL * window, guint width,
guint height)
{
	EglVTable *const vtable = window->egl_vtable;

	vtable->glViewport(0, 0, width, height);
	vtable->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	vtable->glClear(GL_COLOR_BUFFER_BIT);
	return TRUE;
}

static void
do_resize_window(ResizeWindowArgs * args)
{
	GstMfxWindowEGL *const window = args->window;
	EglContextState old_cs;

	GST_MFX_OBJECT_LOCK_DISPLAY(window);
	if (egl_context_set_current(window->egl_window->context, TRUE, &old_cs)) {
		args->success = do_resize_window_unlocked(window, args->width,
			args->height);
		egl_context_set_current(window->egl_window->context, FALSE, &old_cs);
	}
	GST_MFX_OBJECT_UNLOCK_DISPLAY(window);
}

static gboolean
gst_mfx_window_egl_resize(GstMfxWindowEGL * window, guint width,
	guint height)
{
	const GstMfxWindowClass *const klass =
		GST_MFX_WINDOW_GET_CLASS(window->window);
	ResizeWindowArgs args = { window, width, height };

	g_return_val_if_fail(klass->resize, FALSE);

	if (!klass->resize(window->window, width, height))
		return FALSE;

	return egl_context_run(window->egl_window->context,
		(EglContextRunFunc)do_resize_window, &args) && args.success;
}

static gboolean
do_render_texture(GstMfxWindowEGL * window, const GstMfxRectangle * rect)
{
	//const GLuint tex_id = GST_MFX_OBJECT_ID(window->texture);
	GstMfxTextureEGL *const texture = GST_MFX_TEXTURE_EGL(window->texture);
	EglVTable *const vtable = window->egl_vtable;
	GLfloat x0, y0, x1, y1;
	GLfloat texcoords[4][2];
	GLfloat positions[4][2];
	guint tex_width, tex_height;
	uint32_t i;

	if (!ensure_shaders(window))
		return FALSE;

	tex_width = GST_MFX_TEXTURE_WIDTH(window->texture);
	tex_height = GST_MFX_TEXTURE_HEIGHT(window->texture);

	// Source coords in VA surface
	x0 = (GLfloat)rect->x / tex_width;
	y0 = (GLfloat)rect->y / tex_height;
	x1 = (GLfloat)(rect->x + rect->width) / tex_width;
	y1 = (GLfloat)(rect->y + rect->height) / tex_height;
	texcoords[0][0] = x0;
	texcoords[0][1] = y1;
	texcoords[1][0] = x1;
	texcoords[1][1] = y1;
	texcoords[2][0] = x1;
	texcoords[2][1] = y0;
	texcoords[3][0] = x0;
	texcoords[3][1] = y0;

	// Target coords in EGL surface
	x0 = 2.0f * ((GLfloat)rect->x / rect->width) - 1.0f;
	y1 = -2.0f * ((GLfloat)rect->y / rect->height) + 1.0f;
	x1 = 2.0f * ((GLfloat)(rect->x + rect->width) / rect->width) - 1.0f;
	y0 = -2.0f * ((GLfloat)(rect->y + rect->height) / rect->height) + 1.0f;
	positions[0][0] = x0;
	positions[0][1] = y0;
	positions[1][0] = x1;
	positions[1][1] = y0;
	positions[2][0] = x1;
	positions[2][1] = y1;
	positions[3][0] = x0;
	positions[3][1] = y1;

	vtable->glClear(GL_COLOR_BUFFER_BIT);

	/*if (G_UNLIKELY(window->egl_window->context->config->gles_version == 1)) {
		vtable->glBindTexture(GST_MFX_TEXTURE_TARGET(window->texture), tex_id);
		vtable->glEnableClientState(GL_VERTEX_ARRAY);
		vtable->glVertexPointer(2, GL_FLOAT, 0, positions);
		vtable->glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		vtable->glTexCoordPointer(2, GL_FLOAT, 0, texcoords);

		vtable->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		vtable->glDisableClientState(GL_VERTEX_ARRAY);
		vtable->glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	else {*/
		EglProgram *const program = window->render_program;

		vtable->glUseProgram(program->base.handle.u);
		vtable->glUniformMatrix4fv(program->uniforms[RENDER_PROGRAM_VAR_PROJ],
			1, GL_FALSE, window->render_projection);
		vtable->glEnableVertexAttribArray(0);
		vtable->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, positions);
		vtable->glEnableVertexAttribArray(1);
		vtable->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, texcoords);

		for (i = 0; i < texture->num_textures; i++) {
            vtable->glActiveTexture(GL_TEXTURE0 + i);
            vtable->glBindTexture(GST_MFX_TEXTURE_TARGET(window->texture),
                texture->textures[i]);
            vtable->glUniform1i(program->uniforms[i+1], i);
        }

		//vtable->glBindTexture(GST_MFX_TEXTURE_TARGET(window->texture), tex_id);
		//vtable->glUniform1i(program->uniforms[RENDER_PROGRAM_VAR_TEX0], 0);

		vtable->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		vtable->glDisableVertexAttribArray(1);
		vtable->glDisableVertexAttribArray(0);
		vtable->glUseProgram(0);

	//}

	eglSwapBuffers(window->egl_window->context->display->base.handle.p,
		window->egl_window->base.handle.p);

	return TRUE;
}

static gboolean
do_upload_surface_unlocked(GstMfxWindowEGL * window,
	GstMfxSurface * surface, const GstMfxRectangle * src_rect,
	const GstMfxRectangle * dst_rect)
{
	if (!ensure_texture(window, src_rect->width, src_rect->height))
		return FALSE;
	if (!gst_mfx_texture_put_surface(window->texture, surface, src_rect))
		return FALSE;
	if (!do_render_texture(window, src_rect))
		return FALSE;

	return TRUE;
}

static void
do_upload_surface(UploadSurfaceArgs * args)
{
	GstMfxWindowEGL *const window = args->window;
	EglContextState old_cs;

	args->success = FALSE;

	GST_MFX_OBJECT_LOCK_DISPLAY(window);
	if (egl_context_set_current(window->egl_window->context, TRUE, &old_cs)) {
		args->success = do_upload_surface_unlocked(window, args->surface,
			args->src_rect, args->dst_rect);
		egl_context_set_current(window->egl_window->context, FALSE, &old_cs);
	}
	GST_MFX_OBJECT_UNLOCK_DISPLAY(window);
}

static gboolean
gst_mfx_window_egl_render(GstMfxWindowEGL * window,
	GstMfxSurface * surface, const GstMfxRectangle * src_rect,
	const GstMfxRectangle * dst_rect)
{
	UploadSurfaceArgs args = { window, surface, src_rect, dst_rect };

	return egl_context_run(window->egl_window->context,
		(EglContextRunFunc)do_upload_surface, &args) && args.success;
}


/*static gboolean
gst_mfx_window_egl_render_pixmap(GstMfxWindowEGL * window,
	GstMfxPixmap * pixmap,
	const GstMfxRectangle * src_rect, const GstMfxRectangle * dst_rect)
{
	const GstMfxWindowClass *const klass =
		GST_MFX_WINDOW_GET_CLASS(window->window);

	if (!klass->render_pixmap)
		return FALSE;
	return klass->render_pixmap(window->window, pixmap, src_rect, dst_rect);
}*/

void
gst_mfx_window_egl_class_init(GstMfxWindowEGLClass * klass)
{
	GstMfxObjectClass *const object_class = GST_MFX_OBJECT_CLASS(klass);
	GstMfxWindowClass *const window_class = GST_MFX_WINDOW_CLASS(klass);

	object_class->finalize = (GstMfxObjectFinalizeFunc)
		gst_mfx_window_egl_destroy;

	window_class->create = (GstMfxWindowCreateFunc)
		gst_mfx_window_egl_create;
	window_class->show = (GstMfxWindowShowFunc)
		gst_mfx_window_egl_show;
	window_class->hide = (GstMfxWindowHideFunc)
		gst_mfx_window_egl_hide;
	window_class->get_geometry = (GstMfxWindowGetGeometryFunc)
		gst_mfx_window_egl_get_geometry;
	window_class->set_fullscreen = (GstMfxWindowSetFullscreenFunc)
		gst_mfx_window_egl_set_fullscreen;
	window_class->resize = (GstMfxWindowResizeFunc)
		gst_mfx_window_egl_resize;
	window_class->render = (GstMfxWindowRenderFunc)
		gst_mfx_window_egl_render;
	//window_class->render_pixmap = (GstMfxWindowRenderPixmapFunc)
		//gst_mfx_window_egl_render_pixmap;
}

#define gst_mfx_window_egl_finalize \
	gst_mfx_window_egl_destroy

GST_MFX_OBJECT_DEFINE_CLASS_WITH_CODE(GstMfxWindowEGL,
	gst_mfx_window_egl, gst_mfx_window_egl_class_init(&g_class));

/**
* gst_mfx_window_egl_new:
* @display: a #GstMfxDisplay
* @width: the requested window width, in pixels
* @height: the requested windo height, in pixels
*
* Creates a window with the specified @width and @height. The window
* will be attached to the @display and remains invisible to the user
* until gst_mfx_window_show() is called.
*
* Return value: the newly allocated #GstMfxWindow object
*/
GstMfxWindow *
gst_mfx_window_egl_new(GstMfxDisplay * display, guint width, guint height)
{
	GST_DEBUG("new window, size %ux%u", width, height);

	g_return_val_if_fail(GST_MFX_IS_DISPLAY_EGL(display), NULL);

	return
		gst_mfx_window_new_internal(GST_MFX_WINDOW_CLASS
		(gst_mfx_window_egl_class()), display, width,
		height);
}
