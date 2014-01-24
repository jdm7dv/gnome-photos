/*
 * Photos - access, organize and share your photos on GNOME
 * Copyright © 2012, 2013, 2014 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/* Based on code from:
 *   + Documents
 */


#include "config.h"

#include <glib.h>

#include "photos-base-manager.h"
#include "photos-filterable.h"


struct _PhotosBaseManagerPrivate
{
  GHashTable *objects;
  GObject *active_object;
  gchar *title;
};

enum
{
  PROP_0,
  PROP_TITLE
};

enum
{
  ACTIVE_CHANGED,
  CLEAR,
  OBJECT_ADDED,
  OBJECT_REMOVED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (PhotosBaseManager, photos_base_manager, G_TYPE_OBJECT);


static gchar *
photos_base_manager_default_get_where (PhotosBaseManager *self)
{
  return g_strdup ("");
}


static gboolean
photos_base_manager_default_set_active_object (PhotosBaseManager *self, GObject *object)
{
  PhotosBaseManagerPrivate *priv = self->priv;

  if (object == priv->active_object)
    return FALSE;

  g_clear_object (&priv->active_object);

  if (object != NULL)
    g_object_ref (object);

  priv->active_object = object;
  g_signal_emit (self, signals[ACTIVE_CHANGED], 0, object);
  return TRUE;
}


static gchar *
photos_base_manager_get_all_filter (PhotosBaseManager *self)
{
  GList *l;
  GList *values;
  const gchar *blank = "(true)";
  gchar *filter;
  gchar **strv;
  gchar *tmp;
  guint i;
  guint length;

  values = g_hash_table_get_values (self->priv->objects);
  length = g_list_length (values);
  strv = (gchar **) g_malloc0_n (length + 1, sizeof (gchar *));

  for (i = 0, l = values; l != NULL; l = l->next)
    {
      gchar *id;

      g_object_get (l->data, "id", &id, NULL);
      if (g_strcmp0 (id, "all") != 0)
        {
          gchar *str;

          str = photos_filterable_get_filter (PHOTOS_FILTERABLE (l->data));
          if (g_strcmp0 (str, blank) == 0)
            g_free (str);
          else
            {
              strv[i] = str;
              i++;
            }
        }
      g_free (id);
    }

  length = g_strv_length (strv);
  if (length == 0)
    strv[0] = g_strdup (blank);

  filter = g_strjoinv (" || ", strv);
  g_strfreev (strv);

  tmp = filter;
  filter = g_strconcat ("(", filter, ")", NULL);
  g_free (tmp);

  g_list_free (values);
  return filter;
}


static void
photos_base_manager_dispose (GObject *object)
{
  PhotosBaseManager *self = PHOTOS_BASE_MANAGER (object);
  PhotosBaseManagerPrivate *priv = self->priv;

  if (priv->objects != NULL)
    {
      g_hash_table_unref (priv->objects);
      priv->objects = NULL;
    }

  g_clear_object (&priv->active_object);

  G_OBJECT_CLASS (photos_base_manager_parent_class)->dispose (object);
}


static void
photos_base_manager_finalize (GObject *object)
{
  PhotosBaseManager *self = PHOTOS_BASE_MANAGER (object);

  g_free (self->priv->title);

  G_OBJECT_CLASS (photos_base_manager_parent_class)->finalize (object);
}


static void
photos_base_manager_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PhotosBaseManager *self = PHOTOS_BASE_MANAGER (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      self->priv->title = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
photos_base_manager_init (PhotosBaseManager *self)
{
  PhotosBaseManagerPrivate *priv;

  self->priv = photos_base_manager_get_instance_private (self);
  priv = self->priv;

  priv->objects = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}


static void
photos_base_manager_class_init (PhotosBaseManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = photos_base_manager_dispose;
  object_class->finalize = photos_base_manager_finalize;
  object_class->set_property = photos_base_manager_set_property;
  class->get_where = photos_base_manager_default_get_where;
  class->set_active_object = photos_base_manager_default_set_active_object;

  g_object_class_install_property (object_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        "Title",
                                                        "The name of this manager",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  signals[ACTIVE_CHANGED] = g_signal_new ("active-changed",
                                          G_TYPE_FROM_CLASS (class),
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (PhotosBaseManagerClass,
                                                           active_changed),
                                          NULL, /*accumulator */
                                          NULL, /*accu_data */
                                          g_cclosure_marshal_VOID__OBJECT,
                                          G_TYPE_NONE,
                                          1,
                                          G_TYPE_OBJECT);

  signals[CLEAR] = g_signal_new ("clear",
                                 G_TYPE_FROM_CLASS (class),
                                 G_SIGNAL_RUN_LAST,
                                 G_STRUCT_OFFSET (PhotosBaseManagerClass,
                                                  clear),
                                 NULL, /*accumulator */
                                 NULL, /*accu_data */
                                 g_cclosure_marshal_VOID__VOID,
                                 G_TYPE_NONE,
                                 0);

  signals[OBJECT_ADDED] = g_signal_new ("object-added",
                                        G_TYPE_FROM_CLASS (class),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (PhotosBaseManagerClass,
                                                         object_added),
                                        NULL, /*accumulator */
                                        NULL, /* accu_data */
                                        g_cclosure_marshal_VOID__OBJECT,
                                        G_TYPE_NONE,
                                        1,
                                        G_TYPE_OBJECT);

  signals[OBJECT_REMOVED] = g_signal_new ("object-removed",
                                          G_TYPE_FROM_CLASS (class),
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (PhotosBaseManagerClass,
                                                           object_removed),
                                          NULL, /*accumulator */
                                          NULL, /*accu_data */
                                          g_cclosure_marshal_VOID__OBJECT,
                                          G_TYPE_NONE,
                                          1,
                                          G_TYPE_OBJECT);
}


void
photos_base_manager_add_object (PhotosBaseManager *self, GObject *object)
{
  GObject *old_object;
  gchar *id;

  g_object_get (object, "id", &id, NULL);
  old_object = photos_base_manager_get_object_by_id (self, id);
  if (old_object != NULL)
    {
      g_free (id);
      return;
    }

  g_hash_table_insert (self->priv->objects, (gpointer) id, g_object_ref (object));
  g_signal_emit (self, signals[OBJECT_ADDED], 0, object);
}


void
photos_base_manager_clear (PhotosBaseManager *self)
{
  PhotosBaseManagerPrivate *priv = self->priv;

  g_hash_table_remove_all (priv->objects);
  g_clear_object (&priv->active_object);
  g_signal_emit (self, signals[CLEAR], 0);
}


GObject *
photos_base_manager_get_active_object (PhotosBaseManager *self)
{
  return self->priv->active_object;
}


gchar *
photos_base_manager_get_filter (PhotosBaseManager *self)
{
  PhotosBaseManagerPrivate *priv = self->priv;
  const gchar *blank = "(true)";
  gchar *filter;
  gchar *id;

  if (priv->active_object == NULL)
    return g_strdup (blank);

  g_return_val_if_fail (PHOTOS_IS_FILTERABLE (priv->active_object), g_strdup (blank));

  g_object_get (priv->active_object, "id", &id, NULL);
  if (g_strcmp0 (id, "all") == 0)
    filter = photos_base_manager_get_all_filter (self);
  else
    filter = photos_filterable_get_filter (PHOTOS_FILTERABLE (priv->active_object));

  g_free (id);
  return filter;
}


GObject *
photos_base_manager_get_object_by_id (PhotosBaseManager *self, const gchar *id)
{
  return g_hash_table_lookup (self->priv->objects, id);
}


GHashTable *
photos_base_manager_get_objects (PhotosBaseManager *self)
{
  return self->priv->objects;
}


guint
photos_base_manager_get_objects_count (PhotosBaseManager *self)
{
  GList *keys;
  guint count;

  keys = g_hash_table_get_keys (self->priv->objects);
  count = g_list_length (keys);
  g_list_free (keys);
  return count;
}


const gchar *
photos_base_manager_get_title (PhotosBaseManager *self)
{
  return self->priv->title;
}


gchar *
photos_base_manager_get_where (PhotosBaseManager *self)
{
  return PHOTOS_BASE_MANAGER_GET_CLASS (self)->get_where (self);
}


void
photos_base_manager_process_new_objects (PhotosBaseManager *self, GHashTable *new_objects)
{
  GHashTable *old_objects;
  GHashTableIter iter;
  GObject *object;
  const gchar *id;

  old_objects = photos_base_manager_get_objects (self);

  g_hash_table_iter_init (&iter, old_objects);
  while (g_hash_table_iter_next (&iter, (gpointer *) &id, (gpointer *) &object))
    {
      gboolean builtin;

      /* If old objects are not found in the newer hash table, remove
       * them.
       */
      g_object_get (object, "builtin", &builtin, NULL);
      if (g_hash_table_lookup (new_objects, id) == NULL && !builtin)
        {
          g_object_ref (object);
          g_hash_table_iter_remove (&iter);
          g_signal_emit (self, signals[OBJECT_REMOVED], 0, object);
          g_object_unref (object);
        }
    }

  g_hash_table_iter_init (&iter, new_objects);
  while (g_hash_table_iter_next (&iter, (gpointer *) &id, (gpointer *) &object))
    {
      /* If new items are not found in the older hash table, add
       * them.
       */
      if (g_hash_table_lookup (old_objects, id) == NULL)
        photos_base_manager_add_object (self, object);
    }

  /* TODO: merge existing item properties with new values. */
}


void
photos_base_manager_remove_object (PhotosBaseManager *self, GObject *object)
{
  gchar *id;

  g_object_get (object, "id", &id, NULL);
  photos_base_manager_remove_object_by_id (self, id);
  g_free (id);
}


void
photos_base_manager_remove_object_by_id (PhotosBaseManager *self, const gchar *id)
{
  GObject *object;

  object = photos_base_manager_get_object_by_id (self, id);
  if (object == NULL)
    return;

  g_object_ref (object);
  g_hash_table_remove (self->priv->objects, id);
  g_signal_emit (self, signals[OBJECT_REMOVED], 0, object);
  g_object_unref (object);
}


gboolean
photos_base_manager_set_active_object (PhotosBaseManager *self, GObject *object)
{
  return PHOTOS_BASE_MANAGER_GET_CLASS (self)->set_active_object (self, object);
}


gboolean
photos_base_manager_set_active_object_by_id (PhotosBaseManager *self, const gchar *id)
{
  GObject *object;

  object = photos_base_manager_get_object_by_id (self, id);
  return photos_base_manager_set_active_object (self, object);
}
