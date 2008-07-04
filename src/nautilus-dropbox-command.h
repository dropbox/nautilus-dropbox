/*
 * Copyright 2008 Evenflow, Inc.
 *
 * nautilus-dropbox-command.h
 * Header file for nautilus-dropbox-command.c
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

#ifndef NAUTILUS_DROPBOX_COMMAND_H
#define NAUTILUS_DROPBOX_COMMAND_H

#include "nautilus-dropbox.h"

G_BEGIN_DECLS

/* command structs */
typedef enum {GET_FILE_INFO, GENERAL_COMMAND} NautilusDropboxRequestType;

typedef struct {
  NautilusDropboxRequestType request_type;
} DropboxCommand;

typedef struct {
  DropboxCommand dc;
  NautilusInfoProvider *provider;
  GClosure *update_complete;
  NautilusFileInfo *file;
  gboolean cancelled;
} DropboxFileInfoCommand;

typedef struct {
  DropboxFileInfoCommand *dfic;
  GHashTable *file_status_response;
  GHashTable *context_options_response;
} DropboxFileInfoCommandResponse;

typedef void (*NautilusDropboxCommandResponseHandler)(GHashTable *, gpointer);

typedef struct {
  DropboxCommand dc;
  gchar *command_name;
  GHashTable *command_args;
  NautilusDropboxCommandResponseHandler handler;
  gpointer *handler_ud;
} DropboxGeneralCommand;

typedef struct {
  DropboxGeneralCommand *dgc;
  GHashTable *response;
} DropboxGeneralCommandResponse;

gboolean nautilus_dropbox_command_is_connected(NautilusDropbox *cvs);

void nautilus_dropbox_command_force_reconnect(NautilusDropbox *cvs);

gboolean
nautilus_dropbox_command_parse_arg(const gchar *line, GHashTable *return_table);

gchar *nautilus_dropbox_command_sanitize(const gchar *a);

gchar *nautilus_dropbox_command_desanitize(const gchar *a);

void
nautilus_dropbox_command_request(NautilusDropbox *cvs, DropboxCommand *dc);

void
nautilus_dropbox_command_setup(NautilusDropbox *cvs);

void
nautilus_dropbox_command_start(NautilusDropbox *cvs);


G_END_DECLS

#endif
