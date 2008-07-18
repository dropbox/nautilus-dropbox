/*
 * Copyright 2008 Evenflow, Inc.
 *
 * g-util.c
 * glib/gdk/gtk dependent utility functions.
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

#include <glib.h>
#include <glib/gprintf.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "g-util.h"

void
g_util_destroy_string(gpointer data) {
  g_string_free((GString *) data, TRUE);
  return;
}
