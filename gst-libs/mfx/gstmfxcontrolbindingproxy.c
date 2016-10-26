/* 
 * GStreamer
 * Copyright (C) 2016 Matthew Waters <matthew@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "gstmfxcontrolbindingproxy.h"

G_DEFINE_TYPE (GstMfxControlBindingProxy,
    gst_mfx_control_binding_proxy, GST_TYPE_CONTROL_BINDING);

static void
gst_mfx_control_binding_proxy_init (GstMfxControlBindingProxy * self)
{
}

static gboolean
gst_mfx_control_binding_proxy_sync_values (GstControlBinding * binding,
    GstObject * object, GstClockTime timestamp, GstClockTime last_sync)
{
  GstMfxControlBindingProxy *self = (GstMfxControlBindingProxy *)
      binding;
  GstControlBinding *ref_binding;
  gboolean ret = TRUE;

  ref_binding = gst_object_get_control_binding (self->ref_object,
      self->property_name);

  if (ref_binding) {
    ret = gst_control_binding_sync_values (ref_binding, self->ref_object,
        timestamp, last_sync);
    gst_object_unref (ref_binding);
  }

  return ret;
}

static GValue *
gst_mfx_control_binding_proxy_get_value (GstControlBinding * binding,
    GstClockTime timestamp)
{
  GstMfxControlBindingProxy *self = (GstMfxControlBindingProxy *)
      binding;
  GstControlBinding *ref_binding;
  GValue *ret = NULL;

  ref_binding = gst_object_get_control_binding (self->ref_object,
      self->property_name);

  if (ref_binding) {
    ret = gst_control_binding_get_value (ref_binding, timestamp);
    gst_object_unref (ref_binding);
  }

  return ret;
}

static gboolean
gst_mfx_control_binding_proxy_get_value_array (GstControlBinding * binding,
    GstClockTime timestamp, GstClockTime interval, guint n_values,
    gpointer values)
{
  GstMfxControlBindingProxy *self = (GstMfxControlBindingProxy *)
      binding;
  GstControlBinding *ref_binding;
  gboolean ret = FALSE;

  ref_binding = gst_object_get_control_binding (self->ref_object,
      self->property_name);

  if (ref_binding) {
    ret = gst_control_binding_get_value_array (ref_binding, timestamp,
        interval, n_values, values);
    gst_object_unref (ref_binding);
  }

  return ret;
}

static gboolean
gst_mfx_control_binding_proxy_get_g_value_array (GstControlBinding *
    binding, GstClockTime timestamp, GstClockTime interval, guint n_values,
    GValue * values)
{
  GstMfxControlBindingProxy *self = (GstMfxControlBindingProxy *)
      binding;
  GstControlBinding *ref_binding;
  gboolean ret = FALSE;

  ref_binding = gst_object_get_control_binding (self->ref_object,
      self->property_name);

  if (ref_binding) {
    ret = gst_control_binding_get_g_value_array (ref_binding, timestamp,
        interval, n_values, values);
    gst_object_unref (ref_binding);
  }

  return ret;
}


static void
    gst_mfx_control_binding_proxy_class_init
    (GstMfxControlBindingProxyClass * klass)
{
  GstControlBindingClass *cb_class = GST_CONTROL_BINDING_CLASS (klass);

  cb_class->sync_values = gst_mfx_control_binding_proxy_sync_values;
  cb_class->get_value = gst_mfx_control_binding_proxy_get_value;
  cb_class->get_value_array = gst_mfx_control_binding_proxy_get_value_array;
  cb_class->get_g_value_array = gst_mfx_control_binding_proxy_get_g_value_array;
}

GstControlBinding *
gst_mfx_control_binding_proxy_new (GstObject * object,
    const gchar * property_name, GstObject * ref_object,
    const gchar * ref_property_name)
{
  GstMfxControlBindingProxy *self =
      g_object_new (GST_TYPE_MFX_CONTROL_BINDING_PROXY, "object", object,
      "name", property_name, NULL);

  self->ref_object = ref_object;
  self->property_name = ref_property_name;

  return (GstControlBinding *) self;
}

void
gst_mfx_object_add_control_binding_proxy (GstObject * object,
    GstObject * ref_object, const gchar * prop)
{
  GstControlBinding *cb;

  cb = gst_mfx_control_binding_proxy_new (object, prop, ref_object, prop);
  gst_object_add_control_binding (object, cb);
}
