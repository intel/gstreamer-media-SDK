/*
 *  Copyright (C) 2017
 *    Author: Ishmael Visayana Sameen <ishmael1985@gmail.com>
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

#include "gstmfxcontext.h"
#include "d3d11/gstmfxdevice.h"

#define DEBUG 1
#include "gstmfxdebug.h"

/**
* GstMfxContext:
*
* An MFX context wrapper.
*/
struct _GstMfxContext
{
  /*< private > */
  GstObject parent_instance;

  GstMfxDevice * device;
};

G_DEFINE_TYPE(GstMfxContext, gst_mfx_context, GST_TYPE_OBJECT);

static void
gst_mfx_context_finalize (GObject * object)
{
  GstMfxContext* context = GST_MFX_CONTEXT(object);
  
  gst_mfx_device_replace(&context->device, NULL);
}

static void
gst_mfx_context_class_init(GstMfxContextClass * klass)
{
  GObjectClass *const object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = gst_mfx_context_finalize;
}

static void
gst_mfx_context_init(GstMfxContext * context)
{
}

GstMfxContext *
gst_mfx_context_new(GstMfxContext * context, mfxSession session)
{
  context->device =
      gst_mfx_device_new(g_object_new(GST_TYPE_MFX_DEVICE, NULL), session);

  return context;
}

GstMfxContext *
gst_mfx_context_ref (GstMfxContext * context)
{
  g_return_val_if_fail (context != NULL, NULL);

  return gst_object_ref (GST_OBJECT (context));
}

void
gst_mfx_context_unref (GstMfxContext * context)
{
  gst_object_unref (GST_OBJECT(context));
}

void
gst_mfx_context_replace (GstMfxContext ** old_context_ptr,
    GstMfxContext * new_context)
{
  g_return_if_fail (old_context_ptr != NULL);

  gst_object_replace ((GstObject **) old_context_ptr,
	  GST_OBJECT(new_context));
}

GstMfxDevice*
gst_mfx_context_get_device(GstMfxContext * context)
{
  return context->device;
}