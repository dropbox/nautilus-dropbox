/*
 * Copyright 2008 Evenflow, Inc.
 *
 * dropbox.c
 * Nautilus module registering functions for the Dropbox extension.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>

#include "nautilus-dropbox.h"

static GType type_list[1];

void
nautilus_module_initialize (GTypeModule *module) {
  g_print ("Initializing %s\n", PACKAGE_STRING);

  nautilus_dropbox_register_type (module);
  type_list[0] = NAUTILUS_TYPE_DROPBOX;

  dropbox_use_operation_in_progress_workaround = TRUE;
}

void
nautilus_module_shutdown (void) {
  g_print ("Shutting down dropbox extension\n");
}

void
nautilus_module_list_types (const GType **types,
                            int *num_types) {
  *types = type_list;
  *num_types = G_N_ELEMENTS (type_list);
}
