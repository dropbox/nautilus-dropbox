#ifndef NAUTILUS_DROPBOX_HOOKS_H
#define NAUTILUS_DROPBOX_HOOKS_H

#include "nautilus-dropbox.h"

G_BEGIN_DECLS

typedef void (*DropboxUpdateHook)(NautilusDropbox *, GHashTable *);

void
nautilus_dropbox_hooks_setup(NautilusDropbox *cvs);

G_END_DECLS

#endif
