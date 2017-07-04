#ifndef GST_MFX_VALUE_H
#define GST_MFX_VALUE_H

#include <glib-object.h>
#include "gstmfxtypes.h"

G_BEGIN_DECLS

#define GST_MFX_TYPE_OPTION gst_mfx_option_get_type ()

/**
 * GST_MFX_TYPE_ROTATION:
 *
 * A type that represents the MFX surface rotation.
 *
 * Return value: the #GType of GstMfxRotation
 */
#define GST_MFX_TYPE_ROTATION gst_mfx_rotation_get_type ()

/**
 * GST_MFX_TYPE_RATE_CONTROL:
 *
 * A type that represents the MFX rate control algorithm.
 *
 * Return value: the #GType of GstMfxRateControl
 */
#define GST_MFX_TYPE_RATE_CONTROL gst_mfx_rate_control_get_type ()

/**
 * GST_MFX_TYPE_DEINTERLACE_MODE:
 *
 * A type that represents the MFX deinterlace mode.
 *
 * Return value: the #GType of GstMfxDeinterlaceMode
 */
#define GST_MFX_TYPE_DEINTERLACE_METHOD gst_mfx_deinterlace_method_get_type ()

/**
 * GST_MFX_TYPE_FRC_ALGORITHM:
 *
 * A type that represents the MFX frame rate conversion algorithm.
 *
 * Return value: the #GType of GstMfxFrcAlgorithm
 */

#define GST_MFX_TYPE_FRC_ALGORITHM gst_mfx_frc_algorithm_get_type ()

GType
gst_mfx_option_get_type (void);

GType
gst_mfx_rotation_get_type (void);

GType
gst_mfx_rate_control_get_type (void);

GType
gst_mfx_deinterlace_method_get_type(void);

GType
gst_mfx_frc_algorithm_get_type (void);
/**
 * GST_MFX_POPCOUNT32:
 * @x: the value from which to compute population count
 *
 * Computes the number of bits set in the supplied 32-bit value @x.
 *
 * Return value: the number of bits set in @x
 */
#define GST_MFX_POPCOUNT32(x) \
  GST_MFX_POPCOUNT32_0(x)
#define GST_MFX_POPCOUNT32_0(x) \
  GST_MFX_POPCOUNT32_1((x) - (((x) >> 1) & 0x55555555))
#define GST_MFX_POPCOUNT32_1(x) \
  GST_MFX_POPCOUNT32_2(((x) & 0x33333333) + (((x) >> 2) & 0x33333333))
#define GST_MFX_POPCOUNT32_2(x) \
  GST_MFX_POPCOUNT32_3((x) + ((x) >> 4))
#define GST_MFX_POPCOUNT32_3(x) \
  GST_MFX_POPCOUNT32_4((x) & 0x0f0f0f0f)
#define GST_MFX_POPCOUNT32_4(x) \
  (((x) * 0x01010101) >> 24)

/* --- GstMfxEnumSubset --- */

/**
 * GstMfxEnumSubset:
 * @name: name of the enum subset
 * @parent_type: parent enum type
 * @type: registered #GType
 * @type_info: #GTypeInfo used to build the @type
 * @values: pointer to a static array of #GEnumValue elements
 * @num_values: number of elements in the @values array, including the
 *   terminator
 *
 * Structure that holds the required information to build a GEnum
 * subset from the supplied @parent_type, i.e. a subset of its values.
 */
typedef struct {
  GType parent_type;
  GType type;
  GTypeInfo type_info;
  const gchar *type_name;
  GEnumValue *values;
  guint num_values;
} GstMfxEnumSubset;

GType
gst_mfx_type_define_enum_subset_from_mask (GstMfxEnumSubset *subset,
  guint32 mask);

#define GST_MFX_TYPE_DEFINE_ENUM_SUBSET_FROM_MASK(NAME, name, TYPE, MASK) \
  static GType                                                            \
  G_PASTE(name,_get_type)(void)                                           \
{                                                                       	\
  static GEnumValue enum_values[GST_MFX_POPCOUNT32(MASK) + 1];      			\
  static GstMfxEnumSubset subset = {                                			\
  .type_name = G_STRINGIFY(NAME),                                 				\
  .values = enum_values,                                          				\
  .num_values = G_N_ELEMENTS(enum_values),                        				\
};                                                                  			\
  if (g_once_init_enter(&subset.parent_type))                         		\
  g_once_init_leave(&subset.parent_type, TYPE);                   				\
  return gst_mfx_type_define_enum_subset_from_mask(&subset, MASK);  			\
}

G_END_DECLS

#endif /* GST_MFX_VALUE_H */
