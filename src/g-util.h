/*
 * Copyright 2008 Evenflow, Inc.
 *
 * g-util.h
 * Header file for g-util.c
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

#ifndef G_UTIL_H
#define G_UTIL_H

#include <glib.h>
#include <glib/gprintf.h>

G_BEGIN_DECLS

#define debug_enter() {g_print("Entering "); g_print(__FUNCTION__); g_printf("\n");}
#define debug(format, ...) {g_print(__FUNCTION__); g_print(": "); \
    g_printf(format, ## __VA_ARGS__); g_print("\n");}
#define debug_return(v) {g_print("Exiting "); g_print(__FUNCTION__); g_printf("\n"); return v;}

gboolean
g_util_execute_command_line(const gchar *command_line);

void
g_util_destroy_string(gpointer data);

guint g_util_dependable_io_read_watch(GIOChannel *chan,
				      gint priority, 
				      GIOFunc func,
				      gpointer ud,
				      GDestroyNotify notify);

G_END_DECLS

#endif
