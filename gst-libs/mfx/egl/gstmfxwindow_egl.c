/*
 *  Copyright (C) 2014 Intel Corporation
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@intel.com>
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

#include "sysdeps.h"

#include "gstmfxwindow_egl.h"
#include "gstmfxwindow_priv.h"
#include "gstmfxtexture_egl.h"
#include "gstmfxdisplay_egl_priv.h"

#define GST_MFX_WINDOW_EGL(obj) ((GstMfxWindowEGL *)(obj))
#define GST_MFX_WINDOW_EGL_CLASS(klass) ((GstMfxWindowEGLClass *)(klass))
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
  GstMfxTextureEGL *texture;
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
    "  v_texcoord  = texcoord;\n" "}\n";

static const gchar *frag_shader_text_rgba =
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#endif\n"
    "uniform sampler2D tex0;\n"
    "\n"
    "varying vec2 v_texcoord;\n"
    "\n"
    "void main () {\n" "  gl_FragColor = texture2D (tex0, v_texcoord);\n" "}\n";

static gboolean
ensure_texture (GstMfxWindowEGL * window, guint width, guint height)
{
  GstMfxTextureEGL *texture;
  GstMfxDisplay *display = GST_MFX_WINDOW (window)->display;

  texture = gst_mfx_texture_egl_new (display,
      GL_TEXTURE_2D, GL_RGBA, width, height);

  gst_mfx_texture_egl_replace (&window->texture, texture);
  gst_mfx_texture_egl_replace (&texture, NULL);

  return window->texture != NULL;
}

static gboolean
ensure_shaders (GstMfxWindowEGL * window)
{
  EglVTable *const vtable = window->egl_vtable;
  EglProgram *program;
  GLuint prog_id;

  g_return_val_if_fail (window->texture != NULL, FALSE);
  g_return_val_if_fail (GST_MFX_TEXTURE_EGL_FORMAT (window->texture) == GL_RGBA,
      FALSE);

  if (window->render_program)
    return TRUE;

  program = egl_program_new (window->egl_window->context,
      frag_shader_text_rgba, vert_shader_text);
  if (!program)
    return FALSE;

  prog_id = program->base.handle.u;

  vtable->glUseProgram (prog_id);
  program->uniforms[RENDER_PROGRAM_VAR_PROJ] =
      vtable->glGetUniformLocation (prog_id, "proj");
  program->uniforms[RENDER_PROGRAM_VAR_TEX0] =
      vtable->glGetUniformLocation (prog_id, "tex0");
  program->uniforms[RENDER_PROGRAM_VAR_TEX1] =
      vtable->glGetUniformLocation (prog_id, "tex1");
  program->uniforms[RENDER_PROGRAM_VAR_TEX2] =
      vtable->glGetUniformLocation (prog_id, "tex2");
  vtable->glUseProgram (0);

  egl_matrix_set_identity (window->render_projection);

  egl_object_replace (&window->render_program, program);
  egl_object_replace (&program, NULL);
  return TRUE;
}

static gboolean
do_create_objects_unlocked (GstMfxWindowEGL * window, guint width,
    guint height, EglContext * egl_context)
{
  EglWindow *egl_window;
  EglVTable *egl_vtable;

  egl_window = egl_window_new (egl_context,
      GSIZE_TO_POINTER (GST_MFX_WINDOW_ID (window->window)));
  if (!egl_window)
    return FALSE;
  window->egl_window = egl_window;

  egl_vtable = egl_context_get_vtable (egl_window->context, TRUE);
  if (!egl_vtable)
    return FALSE;
  window->egl_vtable = egl_object_ref (egl_vtable);
  return TRUE;
}

static void
do_create_objects (CreateObjectsArgs * args)
{
  GstMfxWindowEGL *const window = args->window;
  EglContextState old_cs;

  args->success = FALSE;

  GST_MFX_DISPLAY_LOCK (GST_MFX_WINDOW_DISPLAY (window));
  if (egl_context_set_current (args->egl_context, TRUE, &old_cs)) {
    args->success = do_create_objects_unlocked (window, args->width,
        args->height, args->egl_context);
    egl_context_set_current (args->egl_context, FALSE, &old_cs);
  }
  GST_MFX_DISPLAY_UNLOCK (GST_MFX_WINDOW_DISPLAY (window));
}

static gboolean
gst_mfx_window_egl_create (GstMfxWindow * window,
    guint * width, guint * height)
{
  GstMfxWindowEGL *mfxEGL_window = GST_MFX_WINDOW_EGL(window);
  GstMfxDisplayEGL *const display =
      GST_MFX_DISPLAY_EGL (GST_MFX_WINDOW_DISPLAY (window));
  const GstMfxDisplayClass *const native_dpy_class =
      GST_MFX_DISPLAY_GET_CLASS (display->display);
  CreateObjectsArgs args;

  g_return_val_if_fail (native_dpy_class != NULL, FALSE);

  mfxEGL_window->window =
      native_dpy_class->create_window (GST_MFX_DISPLAY (display->display),
      GST_MFX_ID_INVALID, *width, *height);
  if (!mfxEGL_window->window)
    return FALSE;

  gst_mfx_window_get_size (mfxEGL_window->window, width, height);

  args.window = mfxEGL_window;
  args.width = *width;
  args.height = *height;
  args.egl_context = GST_MFX_DISPLAY_EGL_CONTEXT (display);
  return egl_context_run (args.egl_context,
      (EglContextRunFunc) do_create_objects, &args) && args.success;
}

static void
do_destroy_objects_unlocked (GstMfxWindowEGL * window)
{
  egl_object_replace (&window->render_program, NULL);
  egl_object_replace (&window->egl_vtable, NULL);
  egl_object_replace (&window->egl_window, NULL);
}

static void
do_destroy_objects (GstMfxWindowEGL * window)
{
  EglContext *const egl_context =
      GST_MFX_DISPLAY_EGL_CONTEXT (GST_MFX_WINDOW_DISPLAY (window));
  EglContextState old_cs;

  if (!window->egl_window)
    return;

  GST_MFX_DISPLAY_LOCK (GST_MFX_WINDOW_DISPLAY (window));
  if (egl_context_set_current (egl_context, TRUE, &old_cs)) {
    do_destroy_objects_unlocked (window);
    egl_context_set_current (egl_context, FALSE, &old_cs);
  }
  GST_MFX_DISPLAY_UNLOCK (GST_MFX_WINDOW_DISPLAY (window));
}

static void
gst_mfx_window_egl_destroy (GstMfxWindow * window)
{
  GstMfxWindowEGL *mfxEGL_window = GST_MFX_WINDOW_EGL(window);

  egl_context_run (mfxEGL_window->egl_window->context,
      (EglContextRunFunc) do_destroy_objects, mfxEGL_window);
  gst_mfx_window_replace (&mfxEGL_window->window, NULL);
  gst_mfx_texture_egl_replace (&mfxEGL_window->texture, NULL);
}

static gboolean
gst_mfx_window_egl_show (GstMfxWindow * window)
{
  GstMfxWindowEGL *mfxEGL_window = GST_MFX_WINDOW_EGL(window);
  const GstMfxWindowClass *const klass =
      GST_MFX_WINDOW_GET_CLASS (mfxEGL_window->window);

  g_return_val_if_fail (klass->show, FALSE);

  return klass->show (mfxEGL_window->window);
}

static gboolean
gst_mfx_window_egl_hide (GstMfxWindow * window)
{
  GstMfxWindowEGL *mfxEGL_window = GST_MFX_WINDOW_EGL(window);
  const GstMfxWindowClass *const klass =
      GST_MFX_WINDOW_GET_CLASS (mfxEGL_window->window);

  g_return_val_if_fail (klass->hide, FALSE);

  return klass->hide (mfxEGL_window->window);
}

static gboolean
gst_mfx_window_egl_get_geometry (GstMfxWindow * window,
    gint * x_ptr, gint * y_ptr, guint * width_ptr, guint * height_ptr)
{
  GstMfxWindowEGL *mfxEGL_window = GST_MFX_WINDOW_EGL(window);
  const GstMfxWindowClass *const klass =
      GST_MFX_WINDOW_GET_CLASS (mfxEGL_window->window);

  return klass->get_geometry ? klass->get_geometry (mfxEGL_window->window,
      x_ptr, y_ptr, width_ptr, height_ptr) : FALSE;
}

static gboolean
gst_mfx_window_egl_set_fullscreen (GstMfxWindow * window,
    gboolean fullscreen)
{
  GstMfxWindowEGL *mfxEGL_window = GST_MFX_WINDOW_EGL(window);
  const GstMfxWindowClass *const klass =
      GST_MFX_WINDOW_GET_CLASS (mfxEGL_window->window);

  return klass->set_fullscreen ? klass->set_fullscreen (mfxEGL_window->window,
      fullscreen) : FALSE;
}

static gboolean
do_resize_window_unlocked (GstMfxWindowEGL * window, guint width, guint height)
{
  EglVTable *const vtable = window->egl_vtable;

  vtable->glViewport (0, 0, width, height);
  vtable->glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
  vtable->glClear (GL_COLOR_BUFFER_BIT);
  return TRUE;
}

static void
do_resize_window (ResizeWindowArgs * args)
{
  GstMfxWindowEGL *const window = args->window;
  EglContextState old_cs;

  GST_MFX_DISPLAY_LOCK (GST_MFX_WINDOW_DISPLAY (window));
  if (egl_context_set_current (window->egl_window->context, TRUE, &old_cs)) {
    args->success = do_resize_window_unlocked (window, args->width,
        args->height);
    egl_context_set_current (window->egl_window->context, FALSE, &old_cs);
  }
  GST_MFX_DISPLAY_UNLOCK (GST_MFX_WINDOW_DISPLAY (window));
}

static gboolean
gst_mfx_window_egl_resize (GstMfxWindow * window, guint width, guint height)
{
  GstMfxWindowEGL *mfxEGL_window = GST_MFX_WINDOW_EGL(window);
  const GstMfxWindowClass *const klass =
      GST_MFX_WINDOW_GET_CLASS (mfxEGL_window->window);
  ResizeWindowArgs args = { mfxEGL_window, width, height };

  g_return_val_if_fail (klass->resize, FALSE);

  if (!klass->resize (mfxEGL_window->window, width, height))
    return FALSE;

  return egl_context_run (mfxEGL_window->egl_window->context,
      (EglContextRunFunc) do_resize_window, &args) && args.success;
}

static gboolean
do_render_texture (GstMfxWindowEGL * window, const GstMfxRectangle * src_rect,
    const GstMfxRectangle * dst_rect)
{
  const GLuint tex_id = GST_MFX_TEXTURE_EGL_ID (window->texture);
  EglVTable *const vtable = window->egl_vtable;
  EglProgram *program;
  GLfloat x0, y0, x1, y1;
  GLfloat texcoords[4][2];
  GLfloat positions[4][2];
  guint tex_width, tex_height, win_width, win_height;

  if (!ensure_shaders (window))
    return FALSE;

  tex_width = GST_MFX_TEXTURE_EGL_WIDTH (window->texture);
  tex_height = GST_MFX_TEXTURE_EGL_HEIGHT (window->texture);
  win_width = dst_rect->width + dst_rect->x * 2;
  win_height = dst_rect->height + dst_rect->y * 2;
  program = window->render_program;

  if(!tex_width || !tex_height)
    return FALSE;
  // Source coords in VA surface
  x0 = (GLfloat) src_rect->x / tex_width;
  y0 = (GLfloat) src_rect->y / tex_height;
  x1 = (GLfloat) (src_rect->x + src_rect->width) / tex_width;
  y1 = (GLfloat) (src_rect->y + src_rect->height) / tex_height;
  texcoords[0][0] = x0;
  texcoords[0][1] = y1;
  texcoords[1][0] = x1;
  texcoords[1][1] = y1;
  texcoords[2][0] = x1;
  texcoords[2][1] = y0;
  texcoords[3][0] = x0;
  texcoords[3][1] = y0;

  // Target coords in EGL surface
  x0 = 2.0f * ((GLfloat) dst_rect->x / win_width) - 1.0f;
  y1 = -2.0f * ((GLfloat) dst_rect->y / win_height) + 1.0f;
  x1 = 2.0f * ((GLfloat) (dst_rect->x + dst_rect->width) / win_width) - 1.0f;
  y0 = -2.0f * ((GLfloat) (dst_rect->y + dst_rect->height) / win_height) + 1.0f;
  positions[0][0] = x0;
  positions[0][1] = y0;
  positions[1][0] = x1;
  positions[1][1] = y0;
  positions[2][0] = x1;
  positions[2][1] = y1;
  positions[3][0] = x0;
  positions[3][1] = y1;

  vtable->glClear (GL_COLOR_BUFFER_BIT);

  vtable->glUseProgram (program->base.handle.u);
  vtable->glUniformMatrix4fv (program->uniforms[RENDER_PROGRAM_VAR_PROJ],
      1, GL_FALSE, window->render_projection);
  vtable->glEnableVertexAttribArray (0);
  vtable->glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, 0, positions);
  vtable->glEnableVertexAttribArray (1);
  vtable->glVertexAttribPointer (1, 2, GL_FLOAT, GL_FALSE, 0, texcoords);

  vtable->glBindTexture (GST_MFX_TEXTURE_EGL_TARGET (window->texture), tex_id);
  vtable->glUniform1i (program->uniforms[RENDER_PROGRAM_VAR_TEX0], 0);

  vtable->glDrawArrays (GL_TRIANGLE_FAN, 0, 4);

  vtable->glDisableVertexAttribArray (1);
  vtable->glDisableVertexAttribArray (0);
  vtable->glUseProgram (0);

  eglSwapBuffers (window->egl_window->context->display->base.handle.p,
      window->egl_window->base.handle.p);

  return TRUE;
}

static gboolean
do_upload_surface_unlocked (GstMfxWindowEGL * window,
    GstMfxSurface * surface, const GstMfxRectangle * src_rect,
    const GstMfxRectangle * dst_rect)
{
  if (!ensure_texture (window, src_rect->width, src_rect->height))
    return FALSE;
  if (!gst_mfx_texture_egl_put_surface (window->texture, surface))
    return FALSE;
  if (!do_render_texture (window, src_rect, dst_rect))
    return FALSE;

  return TRUE;
}

static void
do_upload_surface (UploadSurfaceArgs * args)
{
  GstMfxWindowEGL *const window = args->window;
  EglContextState old_cs;

  args->success = FALSE;

  GST_MFX_DISPLAY_LOCK (GST_MFX_WINDOW_DISPLAY (window));
  if (egl_context_set_current (window->egl_window->context, TRUE, &old_cs)) {
    args->success = do_upload_surface_unlocked (window, args->surface,
        args->src_rect, args->dst_rect);
    egl_context_set_current (window->egl_window->context, FALSE, &old_cs);
  }
  GST_MFX_DISPLAY_UNLOCK (GST_MFX_WINDOW_DISPLAY (window));
}

static gboolean
gst_mfx_window_egl_render (GstMfxWindow * window,
    GstMfxSurface * surface, const GstMfxRectangle * src_rect,
    const GstMfxRectangle * dst_rect)
{
  GstMfxWindowEGL *mfxEGL_window = GST_MFX_WINDOW_EGL(window);
  UploadSurfaceArgs args = { mfxEGL_window, surface, src_rect, dst_rect };

  return egl_context_run (mfxEGL_window->egl_window->context,
      (EglContextRunFunc) do_upload_surface, &args) && args.success;
}

void
gst_mfx_window_egl_class_init (GstMfxWindowEGLClass * klass)
{
  GstMfxMiniObjectClass *const object_class = GST_MFX_MINI_OBJECT_CLASS (klass);
  GstMfxWindowClass *const window_class = GST_MFX_WINDOW_CLASS (klass);

  gst_mfx_window_class_init (&klass->parent_class);

  object_class->size = sizeof (GstMfxWindowEGL);
  window_class->create = gst_mfx_window_egl_create;
  window_class->destroy = gst_mfx_window_egl_destroy;
  window_class->show = gst_mfx_window_egl_show;
  window_class->hide = gst_mfx_window_egl_hide;
  window_class->get_geometry = gst_mfx_window_egl_get_geometry;
  window_class->set_fullscreen = gst_mfx_window_egl_set_fullscreen;
  window_class->resize = gst_mfx_window_egl_resize;
  window_class->render = gst_mfx_window_egl_render;
}

static inline const GstMfxWindowClass *
gst_mfx_window_egl_class (void)
{
  static GstMfxWindowEGLClass g_class;
  static gsize g_class_init = FALSE;

  if (g_once_init_enter (&g_class_init)) {
    gst_mfx_window_egl_class_init (&g_class);
    g_once_init_leave (&g_class_init, TRUE);
  }
  return GST_MFX_WINDOW_CLASS (&g_class);
}

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
gst_mfx_window_egl_new (GstMfxDisplay * display, guint width, guint height)
{
  GST_DEBUG ("new window, size %ux%u", width, height);

  g_return_val_if_fail (GST_MFX_IS_DISPLAY_EGL (display), NULL);

  return
      gst_mfx_window_new_internal (GST_MFX_WINDOW_CLASS
      (gst_mfx_window_egl_class ()), display, GST_MFX_ID_INVALID, width, height);
}

GstMfxWindow *
gst_mfx_window_egl_get_parent_window (GstMfxWindow * window)
{
  g_return_val_if_fail (window != NULL, NULL);

  return GST_MFX_WINDOW_EGL (window)->window;
}
