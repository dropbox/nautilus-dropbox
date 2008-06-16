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

void
nautilus_dropbox_command_setup(NautilusDropbox *cvs);

G_END_DECLS

#endif
