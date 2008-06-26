#ifndef NAUTILUS_DROPBOX_COMMON_H
#define NAUTILUS_DROPBOX_COMMON_H

#include <glib.h>
#include <glib/gprintf.h>

#include "nautilus-dropbox.h"

G_BEGIN_DECLS

typedef void (*NautilusDropboxGlobalCB)(gchar **, NautilusDropbox *, gpointer);

gboolean
nautilus_dropbox_common_execute_command_line(const gchar *);

void
nautilus_dropbox_common_get_globals(NautilusDropbox *cvs,
				    gchar *tabbed_keys,
				    NautilusDropboxGlobalCB cb, gpointer ud);

gboolean
nautilus_dropbox_common_start_dropbox(NautilusDropbox *cvs, gboolean download);

G_END_DECLS

#endif
