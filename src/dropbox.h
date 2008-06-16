#ifndef DROPBOX_H
#define DROPBOX_H

extern gboolean dropbox_use_nautilus_submenu_workaround;

void
nautilus_module_initialized(GTypeModule *module);

void
nautilus_module_shutdown();

void
nautilus_module_list_types();

#endif
