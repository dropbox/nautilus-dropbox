#ifndef NAUTILUS_DROPBOX_COMMON_H
#define NAUTILUS_DROPBOX_COMMON_H

#include <glib.h>
#include <glib/gprintf.h>

#include "nautilus-dropbox.h"

G_BEGIN_DECLS

#define debug_enter() {g_print("Entering "); g_print(__FUNCTION__); g_printf("\n");}
#define debug(format, ...) {g_print(__FUNCTION__); g_print(": "); \
    g_printf(format, ## __VA_ARGS__); g_print("\n");}
#define debug_return(v) {g_print("Exiting "); g_print(__FUNCTION__); \
    g_printf("\n"); return v;}


typedef void (*NautilusDropboxGlobalCB)(gchar **, NautilusDropbox *, gpointer);

gboolean
nautilus_dropbox_common_execute_command_line(const gchar *);

void
nautilus_dropbox_common_destroy_string(gpointer data);

void
nautilus_dropbox_common_get_globals(NautilusDropbox *cvs,
				    gchar *tabbed_keys,
				    NautilusDropboxGlobalCB cb, gpointer ud);

void
nautilus_dropbox_common_start_dropbox(NautilusDropbox *cvs, gboolean download);

G_END_DECLS

#endif
