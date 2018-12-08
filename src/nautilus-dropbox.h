/*
 * Copyright 2008 Evenflow, Inc.
 *
 * nautilus-dropbox.h
 * Header file for nautilus-dropbox.c
 *
 * This file is part of nautilus-dropbox.
 *
 * nautilus-dropbox is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * nautilus-dropbox is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with nautilus-dropbox.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef NAUTILUS_DROPBOX_H
#define NAUTILUS_DROPBOX_H

#include <glib.h>
#include <glib-object.h>

#include <nautilus-extension.h>

#include "dropbox-command-client.h"
#include "nautilus-dropbox-hooks.h"
#include "dropbox-client.h"

G_BEGIN_DECLS

/* Declarations for the dropbox extension object.  This object will be
 * instantiated by nautilus.  It implements the GInterfaces 
 * exported by libnautilus. */

#define NAUTILUS_TYPE_DROPBOX	  (nautilus_dropbox_get_type ())
#define NAUTILUS_DROPBOX(o)	  (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_DROPBOX, NautilusDropbox))
#define NAUTILUS_IS_DROPBOX(o)	  (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_DROPBOX))
typedef struct _NautilusDropbox      NautilusDropbox;
typedef struct _NautilusDropboxClass NautilusDropboxClass;

struct _NautilusDropbox {
  GObject parent_slot;
  GHashTable *filename2obj;
  GHashTable *obj2filename;
  GMutex *emblem_paths_mutex;
  GHashTable *emblem_paths;
  DropboxClient dc;
};

struct _NautilusDropboxClass {
	GObjectClass parent_slot;
};

GType nautilus_dropbox_get_type(void);
void  nautilus_dropbox_register_type(GTypeModule *module);

extern gboolean dropbox_use_nautilus_submenu_workaround;
extern gboolean dropbox_use_operation_in_progress_workaround;

G_END_DECLS

#endif
