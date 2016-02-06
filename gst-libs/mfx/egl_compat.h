#ifndef EGL_COMPAT_H
#define EGL_COMPAT_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "ogl_compat.h"

#ifndef GL_OES_EGL_image
#define GL_OES_EGL_image 1
typedef void *GLeglImageOES;
typedef void(*PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)(GLenum target,
	GLeglImageOES image);
typedef void(*PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC)(GLenum target,
	GLeglImageOES image);
#endif /* GL_OES_EGL_image */

#endif /* EGL_COMPAT_H */
