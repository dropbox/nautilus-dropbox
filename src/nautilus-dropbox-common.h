/*
 * Copyright 2008 Evenflow, Inc.
 *
 * nautilus-dropbox-common.h
 * Header file for nautilus-dropbox-common.h
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

#ifndef NAUTILUS_DROPBOX_COMMON_H
#define NAUTILUS_DROPBOX_COMMON_H

#include <glib.h>
#include <glib/gprintf.h>

#include "nautilus-dropbox.h"

G_BEGIN_DECLS

typedef void (*NautilusDropboxGlobalCB)(gchar **, gpointer);

void
nautilus_dropbox_common_get_globals(DropboxCommandClient *dcc,
				    gchar *tabbed_keys,
				    NautilusDropboxGlobalCB cb, gpointer ud);

gboolean
nautilus_dropbox_common_start_dropbox();

gchar *
nautilus_dropbox_common_get_platform();

G_END_DECLS

#endif
