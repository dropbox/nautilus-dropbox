/*
 *
 *  dropbox.c
 * 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <libnotify/notification.h>

#include "nautilus-dropbox-common.h"
#include "nautilus-dropbox.h"

static GType type_list[1];

void
nautilus_module_initialize (GTypeModule *module) {
  g_print ("Initializing dropbox extension\n");
  
  nautilus_dropbox_register_type (module);
  type_list[0] = NAUTILUS_TYPE_DROPBOX;

  dropbox_use_nautilus_submenu_workaround
    = (NAUTILUS_VERSION_MAJOR < 2 ||
       (NAUTILUS_VERSION_MAJOR == 2 && NAUTILUS_VERSION_MINOR <= 22));
}

void
nautilus_module_shutdown (void) {
  g_print ("Shutting down dropbox extension\n");
}

void 
nautilus_module_list_types (const GType **types,
                            int *num_types) {
  *types = type_list;
  *num_types = G_N_ELEMENTS (type_list);
}
