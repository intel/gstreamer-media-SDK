/*
 *  Copyright (C) 2010-2011 Splitted-Desktop Systems
 *    Author: Gwenole Beauchesne <gwenole.beauchesne@splitted-desktop.com>
 *  Copyright (C) 2012-2014 Intel Corporation
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
#include <gobject/gvaluecollector.h>
#include "gstmfxvalue.h"

GType
gst_mfx_option_get_type (void)
{
  static volatile gsize g_type = 0;

  static const GEnumValue options[] = {
    {GST_MFX_OPTION_AUTO,
        "Let Media SDK decide", "auto"},
    {GST_MFX_OPTION_ON,
        "Turn option on", "on"},
    {GST_MFX_OPTION_OFF,
        "Turn option off", "off"},
    {0, NULL, NULL},
  };

  if (g_once_init_enter (&g_type)) {
    GType type = g_enum_register_static ("GstMfxOption", options);
    g_once_init_leave (&g_type, type);
  }
  return g_type;
}

GType
gst_mfx_rate_control_get_type (void)
{
  static volatile gsize g_type = 0;

  static const GEnumValue rate_control_values[] = {
    {GST_MFX_RATECONTROL_NONE,
        "None", "none"},
    {GST_MFX_RATECONTROL_CQP,
        "Constant QP", "cqp"},
    {GST_MFX_RATECONTROL_CBR,
        "Constant bitrate", "cbr"},
    {GST_MFX_RATECONTROL_VCM,
        "Video conference mode", "vcm"},
    {GST_MFX_RATECONTROL_VBR,
        "Variable bitrate", "vbr"},
    {GST_MFX_RATECONTROL_AVBR,
        "Average variable bitrate", "avbr"},
    {GST_MFX_RATECONTROL_QVBR,
        "Quality variable bitrate", "qvbr"},
    {GST_MFX_RATECONTROL_LA_BRC,
        "Bitrate control with look-ahead", "la-brc"},
    {GST_MFX_RATECONTROL_ICQ,
        "Intelligent constant quality", "icq"},
    {GST_MFX_RATECONTROL_LA_ICQ,
        "Intelligent constant quality with look-ahead", "la-icq"},
    {GST_MFX_RATECONTROL_LA_HRD,
        "HRD-compliant bit rate coding with look-ahead", "la-hrd"},
    {0, NULL, NULL},
  };

  if (g_once_init_enter (&g_type)) {
    GType type = g_enum_register_static ("GstMfxRateControl",
        rate_control_values);
    g_once_init_leave (&g_type, type);
  }
  return g_type;
}

static gboolean
build_enum_subset_values_from_mask (GstMfxEnumSubset * subset, guint32 mask)
{
  GEnumClass *enum_class;
  const GEnumValue *value;
  guint i, n;

  enum_class = g_type_class_ref (subset->parent_type);
  if (!enum_class)
    return FALSE;

  for (i = 0, n = 0; i < 32 && n < subset->num_values; i++) {
    if (!(mask & (1U << i)))
      continue;
    value = g_enum_get_value (enum_class, i);
    if (!value)
      continue;
    subset->values[n++] = *value;
  }
  g_type_class_unref (enum_class);
  if (n != subset->num_values - 1)
    goto error_invalid_num_values;
  return TRUE;

  /* ERRORS */
error_invalid_num_values:
  {
    g_error ("invalid number of static values for `%s'", subset->type_name);
    return FALSE;
  }
}

GType
gst_mfx_type_define_enum_subset_from_mask (GstMfxEnumSubset * subset,
    guint32 mask)
{
  if (g_once_init_enter (&subset->type)) {
    GType type;

    build_enum_subset_values_from_mask (subset, mask);
    memset (&subset->type_info, 0, sizeof (subset->type_info));
    g_enum_complete_type_info (subset->parent_type, &subset->type_info,
        subset->values);

    type = g_type_register_static (G_TYPE_ENUM, subset->type_name,
        &subset->type_info, 0);
    g_once_init_leave (&subset->type, type);
  }
  return subset->type;
}
