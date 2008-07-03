/*
 * nautilus-dropbox-hooks.h
 * Header file for nautilus-dropbox-hooks.c
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

#ifndef NAUTILUS_DROPBOX_HOOKS_H
#define NAUTILUS_DROPBOX_HOOKS_H

#include "nautilus-dropbox.h"

G_BEGIN_DECLS

typedef void (*DropboxUpdateHook)(NautilusDropbox *, GHashTable *);

void
nautilus_dropbox_hooks_setup(NautilusDropbox *cvs);

void
nautilus_dropbox_hooks_start(NautilusDropbox *cvs);

void
nautilus_dropbox_hooks_wait_until_connected(NautilusDropbox *cvs, gboolean val);

gboolean
nautilus_dropbox_hooks_force_reconnect(NautilusDropbox *cvs);

G_END_DECLS

#endif
