#ifndef NAUTILUS_DROPBOX_TRAY_H
#define NAUTILUS_DROPBOX_TRAY_H

#include "nautilus-dropbox.h"

G_BEGIN_DECLS

void
nautilus_dropbox_tray_setup(NautilusDropbox *cvs);

void
nautilus_dropbox_tray_on_connect(NautilusDropbox *cvs);

void
nautilus_dropbox_tray_on_disconnect(NautilusDropbox *cvs);

void
nautilus_dropbox_tray_start_dropbox_transfer(NautilusDropbox *cvs);

G_END_DECLS

#endif
