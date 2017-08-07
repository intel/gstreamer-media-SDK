/*
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

#include "gstmfxsurfacepool.h"
#include "gstmfxsurface.h"

#ifdef WITH_LIBVA_BACKEND
# include "gstmfxsurface_vaapi.h"
#else
# include "gstmfxsurface_d3d11.h"
#endif

#define DEBUG 1
#include "gstmfxdebug.h"

struct _GstMfxSurfacePool
{
  /*< private > */
  GstObject parent_instance;

  GstMfxContext *context;
  GstMfxTask *task;
  GstVideoInfo info;
  gboolean memtype_is_system;
  GQueue free_surfaces;
  GList *used_surfaces;
  guint used_count;
  GMutex mutex;
};

G_DEFINE_TYPE(GstMfxSurfacePool, gst_mfx_surface_pool, GST_TYPE_OBJECT);

static void
gst_mfx_surface_pool_put_surface (GstMfxSurfacePool * pool,
    GstMfxSurface * surface);

static gint
sync_output_surface (gconstpointer surface, gconstpointer surf)
{
  GstMfxSurface *_surface = (GstMfxSurface *) surface;
  mfxFrameSurface1 *_surf = (mfxFrameSurface1 *) surf;

  return _surf != GST_MFX_SURFACE_FRAME_SURFACE (_surface);
}

static void
release_surfaces (gpointer surface, gpointer pool)
{
  GstMfxSurface *_surface = (GstMfxSurface *) surface;
  GstMfxSurfacePool *_pool = (GstMfxSurfacePool *) pool;

  mfxFrameSurface1 *surf = gst_mfx_surface_get_frame_surface (_surface);
  if (surf && !surf->Data.Locked)
    gst_mfx_surface_pool_put_surface (_pool, _surface);
}

static gboolean
gst_mfx_surface_pool_add_surfaces(GstMfxSurfacePool * pool)
{
  guint i, num_surfaces = gst_mfx_task_get_num_surfaces(pool->task);
  GstMfxSurface *surface = NULL;

  for (i = 0; i < num_surfaces; i++) {
#ifdef WITH_LIBVA_BACKEND
    surface = gst_mfx_surface_vaapi_new_from_task(pool->task);
#else
    surface = gst_mfx_surface_d3d11_new_from_task(pool->task);

    if (gst_mfx_task_has_type(pool->task,
        GST_MFX_TASK_VPP_OUT | GST_MFX_TASK_DECODER))
      gst_mfx_surface_d3d11_set_rw_flags (
        GST_MFX_SURFACE_D3D11 (surface), MFX_SURFACE_READ);
#endif // WITH_LIBVA_BACKEND
    if (!surface)
      return FALSE;

    g_queue_push_tail(&pool->free_surfaces, surface);
  }
  return TRUE;
}

static gboolean
gst_mfx_surface_pool_create (GstMfxSurfacePool * pool)
{
  if (pool->task && gst_mfx_task_has_video_memory (pool->task))
    if (!gst_mfx_surface_pool_add_surfaces (pool))
      return FALSE;
  return TRUE;
}

static void
gst_mfx_surface_pool_finalize (GObject * object)
{
  GstMfxSurfacePool *pool = GST_MFX_SURFACE_POOL (object);
  GstMfxSurface *surface;

  while (g_list_length (pool->used_surfaces)) {
    surface = g_list_nth_data (pool->used_surfaces, 0);
    gst_mfx_surface_pool_put_surface (pool, surface);
  }

  g_list_free (pool->used_surfaces);
  g_queue_foreach (&pool->free_surfaces,
      (GFunc) gst_mfx_surface_unref, NULL);
  g_queue_clear (&pool->free_surfaces);
  g_mutex_clear (&pool->mutex);

  gst_mfx_task_replace (&pool->task, NULL);
  gst_mfx_context_replace (&pool->context, NULL);
}

GstMfxSurfacePool *
gst_mfx_surface_pool_new (GstMfxContext * context,
  const GstVideoInfo * info, gboolean memtype_is_system)
{
  GstMfxSurfacePool * pool;

  g_return_val_if_fail(info != NULL, NULL);

  pool = g_object_new(GST_TYPE_MFX_SURFACE_POOL, NULL);
  if (!pool)
    return NULL;

  pool->context = context ? gst_mfx_context_ref(context) : NULL;
  pool->memtype_is_system = memtype_is_system;
  pool->info = *info;
  if (!gst_mfx_surface_pool_create(pool))
    goto error;
  return pool;

error:
  gst_mfx_surface_pool_unref(pool);
  return NULL;
}

GstMfxSurfacePool *
gst_mfx_surface_pool_new_with_task (GstMfxTask * task)
{
  GstMfxSurfacePool * pool;

  g_return_val_if_fail(task != NULL, NULL);

  pool = g_object_new(GST_TYPE_MFX_SURFACE_POOL, NULL);
  if (!pool)
    return NULL;

  pool->task = gst_mfx_task_ref (task);
  pool->memtype_is_system = !gst_mfx_task_has_video_memory (task);
  if (!gst_mfx_surface_pool_create(pool))
    goto error;
  return pool;

error:
  gst_mfx_surface_pool_unref(pool);
  return NULL;
}

GstMfxSurfacePool *
gst_mfx_surface_pool_ref (GstMfxSurfacePool * pool)
{
  g_return_val_if_fail (pool != NULL, NULL);

  return gst_object_ref (GST_OBJECT (pool));
}

void
gst_mfx_surface_pool_unref (GstMfxSurfacePool * pool)
{
  gst_object_unref (GST_OBJECT(pool));
}

void
gst_mfx_surface_pool_replace (GstMfxSurfacePool ** old_pool_ptr,
    GstMfxSurfacePool * new_pool)
{
  g_return_if_fail (old_pool_ptr != NULL);

  gst_object_replace ((GstObject **) old_pool_ptr, GST_OBJECT (new_pool));
}


static void
gst_mfx_surface_pool_put_surface_unlocked (GstMfxSurfacePool * pool,
    GstMfxSurface * surface)
{
  GList *elem;

  elem = g_list_find (pool->used_surfaces, surface);
  if (!elem)
    return;

  gst_mfx_surface_unref (surface);
  --pool->used_count;
  pool->used_surfaces = g_list_delete_link (pool->used_surfaces, elem);
  g_queue_push_tail (&pool->free_surfaces, surface);
}

static void
gst_mfx_surface_pool_put_surface (GstMfxSurfacePool * pool,
    GstMfxSurface * surface)
{
  g_return_if_fail (pool != NULL);
  g_return_if_fail (surface != NULL);

  g_mutex_lock (&pool->mutex);
  gst_mfx_surface_pool_put_surface_unlocked (pool, surface);
  g_mutex_unlock (&pool->mutex);
}

static GstMfxSurface *
gst_mfx_surface_pool_get_surface_unlocked (GstMfxSurfacePool * pool)
{
  GstMfxSurface *surface;

  surface = g_queue_pop_head (&pool->free_surfaces);
  if (!surface) {
    g_mutex_unlock (&pool->mutex);
    if (pool->task) {
      surface = gst_mfx_surface_new_from_task (pool->task);
    }
    else {
      if (!pool->memtype_is_system)
#ifdef WITH_LIBVA_BACKEND
        surface = gst_mfx_surface_vaapi_new (pool->context, &pool->info);
#else
        surface = gst_mfx_surface_d3d11_new (pool->context, &pool->info);
#endif
      else
        surface = gst_mfx_surface_new (&pool->info);
    }

    g_mutex_lock (&pool->mutex);
    if (!surface)
      return NULL;
  }

  ++pool->used_count;
  pool->used_surfaces = g_list_prepend (pool->used_surfaces, surface);

  return gst_mfx_surface_ref (surface);
}

GstMfxSurface *
gst_mfx_surface_pool_get_surface (GstMfxSurfacePool * pool)
{
  GstMfxSurface *surface;

  g_return_val_if_fail (pool != NULL, NULL);

  g_list_foreach (pool->used_surfaces, release_surfaces, pool);

  g_mutex_lock (&pool->mutex);
  surface = gst_mfx_surface_pool_get_surface_unlocked (pool);
  g_mutex_unlock (&pool->mutex);

  return surface;
}

GstMfxSurface *
gst_mfx_surface_pool_find_surface (GstMfxSurfacePool * pool,
    mfxFrameSurface1 * surface)
{
  g_return_val_if_fail (pool != NULL, NULL);

  GList *l = g_list_find_custom (pool->used_surfaces, surface,
      sync_output_surface);

  return GST_MFX_SURFACE (l->data);
}

static void
gst_mfx_surface_pool_init(GstMfxSurfacePool * pool)
{
  pool->used_surfaces = NULL;
  pool->used_count = 0;

  g_queue_init(&pool->free_surfaces);
  g_mutex_init(&pool->mutex);
}

static void
gst_mfx_surface_pool_class_init(GstMfxSurfacePoolClass * klass)
{
  GObjectClass *const object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = gst_mfx_surface_pool_finalize;
}
