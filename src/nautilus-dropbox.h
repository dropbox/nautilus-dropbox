/*
 *  nautilus-dropbox.h
 * 
 */

#ifndef NAUTILUS_DROPBOX_H
#define NAUTILUS_DROPBOX_H

#include <glib.h>
#include <glib-object.h>
#include <libnotify/notification.h>

#include <libnautilus-extension/nautilus-info-provider.h>

G_BEGIN_DECLS

/* Declarations for the dropbox extension object.  This object will be
 * instantiated by nautilus.  It implements the GInterfaces 
 * exported by libnautilus. */

#define NAUTILUS_TYPE_DROPBOX	  (nautilus_dropbox_get_type ())
#define NAUTILUS_DROPBOX(o)	  (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_DROPBOX, NautilusDropbox))
#define NAUTILUS_IS_DROPBOX(o)	  (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_DROPBOX))
typedef struct _NautilusDropbox      NautilusDropbox;
typedef struct _NautilusDropboxClass NautilusDropboxClass;

typedef struct {
  GtkStatusIcon *status_icon;
  GtkMenu *context_menu;
  NotifyNotification *bubble;
  GdkPixbuf *idle;
  GdkPixbuf *busy;
  GdkPixbuf *busy2;
  gint icon_state;
  gint busy_frame;
  gboolean last_active;
} NautilusDropboxTray;

typedef struct {
  GIOChannel *chan;
  int socket;
  struct {
    int line;
    gchar *command_name;
    GHashTable *command_args;
  } hhsi;
  GCond *connected_cond;
  GMutex *connected_mutex;
  gboolean connected;
  guint event_source;
} NautilusDropboxHookserv;

struct _NautilusDropbox {
  GObject parent_slot;
  GAsyncQueue *command_queue;
  GHashTable *dispatch_table;
  GList *file_store;
  GMutex *command_connected_mutex;
  gboolean command_connected;
  NautilusDropboxTray ndt;
  NautilusDropboxHookserv hookserv;
  struct {
    gboolean user_quit;
    gboolean dropbox_starting;
  } ca;
};

struct _NautilusDropboxClass {
	GObjectClass parent_slot;
};

GType nautilus_dropbox_get_type      (void);
void  nautilus_dropbox_register_type (GTypeModule *module);

extern gboolean dropbox_use_nautilus_submenu_workaround;

void nautilus_dropbox_on_connect(NautilusDropbox *cvs);

void nautilus_dropbox_on_disconnect(NautilusDropbox *cvs);


G_END_DECLS

#endif
