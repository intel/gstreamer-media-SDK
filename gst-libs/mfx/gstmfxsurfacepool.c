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
#include "gstmfxsurface_vaapi.h"
#include "gstmfxminiobject.h"

#define DEBUG 1
#include "gstmfxdebug.h"

struct _GstMfxSurfacePool
{
  /*< private > */
  GstMfxMiniObject parent_instance;

  GstMfxDisplay *display;
  GstMfxTask *task;
  GstVideoInfo info;
  gboolean memtype_is_system;
  GQueue free_surfaces;
  GList *used_surfaces;
  guint used_count;
  GMutex mutex;
};

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

static void
gst_mfx_surface_pool_add_surfaces(GstMfxSurfacePool * pool)
{
  guint i, num_surfaces = gst_mfx_task_get_num_surfaces(pool->task);
  GstMfxSurface *surface;

  for (i = 0; i < num_surfaces; i++) {
    surface = gst_mfx_surface_vaapi_new_from_task (pool->task);
    if (!surface)
      return;

    g_queue_push_tail (&pool->free_surfaces, surface);
  }
}

static void
gst_mfx_surface_pool_init (GstMfxSurfacePool * pool)
{
  pool->used_surfaces = NULL;
  pool->used_count = 0;

  g_queue_init (&pool->free_surfaces);
  g_mutex_init (&pool->mutex);

  if (pool->task && gst_mfx_task_has_video_memory (pool->task))
    gst_mfx_surface_pool_add_surfaces(pool);
}

void
gst_mfx_surface_pool_finalize (GstMfxSurfacePool * pool)
{
  GstMfxSurface *surface;

  while (g_list_length(pool->used_surfaces)) {
    surface = g_list_nth_data (pool->used_surfaces, 0);
    gst_mfx_surface_pool_put_surface(pool, surface);
  }

  g_list_free (pool->used_surfaces);
  g_queue_foreach (&pool->free_surfaces,
      (GFunc) gst_mfx_surface_unref, NULL);
  g_queue_clear (&pool->free_surfaces);
  g_mutex_clear (&pool->mutex);

  gst_mfx_display_replace(&pool->display, NULL);
  gst_mfx_task_replace (&pool->task, NULL);
}

static inline const GstMfxMiniObjectClass *
gst_mfx_surface_pool_class (void)
{
  static const GstMfxMiniObjectClass GstMfxSurfacePoolClass = {
    sizeof (GstMfxSurfacePool),
    (GDestroyNotify) gst_mfx_surface_pool_finalize
  };
  return &GstMfxSurfacePoolClass;
}

GstMfxSurfacePool *
gst_mfx_surface_pool_new (GstMfxDisplay * display,
    const GstVideoInfo * info, gboolean memtype_is_system)
{
  GstMfxSurfacePool *pool;

  g_return_val_if_fail (info != NULL, NULL);

  pool = (GstMfxSurfacePool *)
      gst_mfx_mini_object_new0 (gst_mfx_surface_pool_class ());
  if (!pool)
    return NULL;

  pool->display = gst_mfx_display_ref(display);
  pool->memtype_is_system = memtype_is_system;
  pool->info = *info;

  gst_mfx_surface_pool_init (pool);

  return pool;
}

GstMfxSurfacePool *
gst_mfx_surface_pool_new_with_task (GstMfxTask * task)
{
  GstMfxSurfacePool *pool;

  g_return_val_if_fail (task != NULL, NULL);

  pool = (GstMfxSurfacePool *)
      gst_mfx_mini_object_new0 (gst_mfx_surface_pool_class ());
  if (!pool)
    return NULL;

  pool->task = gst_mfx_task_ref (task);
  pool->memtype_is_system = !gst_mfx_task_has_video_memory (task);
  gst_mfx_surface_pool_init (pool);

  return pool;
}

GstMfxSurfacePool *
gst_mfx_surface_pool_ref (GstMfxSurfacePool * pool)
{
  g_return_val_if_fail (pool != NULL, NULL);

  return (GstMfxSurfacePool *) gst_mfx_mini_object_ref (GST_MFX_MINI_OBJECT (pool));
}

void
gst_mfx_surface_pool_unref (GstMfxSurfacePool * pool)
{
  gst_mfx_mini_object_unref (GST_MFX_MINI_OBJECT (pool));
}

void
gst_mfx_surface_pool_replace (GstMfxSurfacePool ** old_pool_ptr,
    GstMfxSurfacePool * new_pool)
{
  g_return_if_fail (old_pool_ptr != NULL);

  gst_mfx_mini_object_replace ((GstMfxMiniObject **) old_pool_ptr,
      GST_MFX_MINI_OBJECT (new_pool));
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
        surface = gst_mfx_surface_vaapi_new(pool->display, &pool->info, NULL);
      else
        surface = gst_mfx_surface_new(&pool->info);
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
