/*
 *
 *  nautilus-dropbox-tray.c
 *
 */

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <glib-object.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <libnotify/notify.h>

#include "nautilus-dropbox.h"
#include "nautilus-dropbox-common.h"
#include "nautilus-dropbox-command.h"
#include "nautilus-dropbox-hooks.h"

#include "busy2.h"
#include "busy.h"
#include "idle.h"

typedef struct {
  NautilusDropbox *cvs;
  gchar *command;
} MenuItemData;

static void
activate_open_my_dropbox(GtkStatusIcon *status_icon,
			 NautilusDropbox *cvs) {
  if (nautilus_dropbox_command_is_connected(cvs) == TRUE) {
    DropboxGeneralCommand *dgc = g_new(DropboxGeneralCommand, 1);
    
    dgc->dc.request_type = GENERAL_COMMAND;
    dgc->command_name = g_strdup("tray_action_open_dropbox");
    dgc->command_args = NULL;
    dgc->handler = NULL;
    dgc->handler_ud = NULL;
    
    nautilus_dropbox_command_request(cvs, (DropboxCommand *) dgc);
  }
}

static void
activate_menu_item(GtkStatusIcon *status_icon,
		   MenuItemData *mid) {
  if (nautilus_dropbox_command_is_connected(mid->cvs) == TRUE) {
    DropboxGeneralCommand *dgc = g_new(DropboxGeneralCommand, 1);
    
    dgc->dc.request_type = GENERAL_COMMAND;
    dgc->command_name = g_strdup(mid->command);
    dgc->command_args = NULL;
    dgc->handler = NULL;
    dgc->handler_ud = NULL;
    
    nautilus_dropbox_command_request(mid->cvs, (DropboxCommand *) dgc);
  }
}

void menu_item_data_destroy(MenuItemData *mid,
			    GClosure *closure) {
  g_free(mid->command);
  g_free(mid);
}

static void
build_context_menu_from_list(NautilusDropbox *cvs, gchar **options_arr) {
  int i;

  cvs->ndt.context_menu = GTK_MENU(gtk_menu_new());

  for (i = 0; options_arr[i] != NULL; i+=2) {
    GtkWidget *item;
    
    if (strcmp(options_arr[i], "") == 0) {
      item = gtk_separator_menu_item_new();
      gtk_menu_shell_append(GTK_MENU_SHELL(cvs->ndt.context_menu), item);
    }
    else {
      item = gtk_menu_item_new_with_label(options_arr[i]);
      gtk_menu_shell_append(GTK_MENU_SHELL(cvs->ndt.context_menu), item);

      if (strcmp(options_arr[i+1], "") == 0) {
	g_object_set(item, "sensitive", FALSE, NULL);
      }
      else {
	MenuItemData *mid = g_new0(MenuItemData, 1);
	mid->cvs = cvs;
	mid->command = g_strdup(options_arr[i+1]);
	g_signal_connect_data(G_OBJECT(item), "activate",
			      G_CALLBACK(activate_menu_item), 
			      mid, (GClosureNotify) menu_item_data_destroy, 0);
      }
    }
  }

  /* now add the quit dropbox item */
  {
    GtkWidget *item;
    MenuItemData *mid = g_new0(MenuItemData, 1);
    mid->cvs = cvs;
    mid->command = g_strdup("tray_action_hard_exit");
    item = gtk_menu_item_new_with_label("Stop Dropbox");
    gtk_menu_shell_append(GTK_MENU_SHELL(cvs->ndt.context_menu), item);
    g_signal_connect_data(G_OBJECT(item), "activate",
			  G_CALLBACK(activate_menu_item), 
			  mid, (GClosureNotify) menu_item_data_destroy, 0);
  }
}

static void
get_menu_options_response_cb(GHashTable *response, NautilusDropbox *cvs) {
  /* great got the menu options, now build the menu and show the tray icon*/

  /*debug_enter(); */

  GString *options_key;
  GString *options;

  options_key = g_string_new("options");

  if ((options = g_hash_table_lookup(response, options_key)) != NULL) {
    gchar **options_arr;

    options_arr = g_strsplit(options->str, "\t", 0);
    if (cvs->ndt.context_menu != NULL) gtk_object_destroy(GTK_OBJECT(cvs->ndt.context_menu));
    build_context_menu_from_list(cvs, options_arr);

    gtk_status_icon_set_visible(cvs->ndt.status_icon, TRUE); 
  }

  g_string_free(options_key, TRUE);
}

static void
send_for_menu(NautilusDropbox *cvs, gboolean active,
	      NautilusDropboxCommandResponseHandler h) {
  DropboxGeneralCommand *dgc = g_new(DropboxGeneralCommand, 1);
  
  dgc->dc.request_type = GENERAL_COMMAND;
  dgc->command_name = g_strdup("tray_action_get_menu_options");

  {
    dgc->command_args = g_hash_table_new_full((GHashFunc) g_string_hash,
					      (GEqualFunc) g_string_equal,
					      nautilus_dropbox_common_destroy_string,
					      nautilus_dropbox_common_destroy_string);
    g_hash_table_insert(dgc->command_args,
			g_string_new("is_active"),
			g_string_new(active ? "true" : "false"));
  }
  dgc->handler = (NautilusDropboxCommandResponseHandler) get_menu_options_response_cb;
  dgc->handler_ud = (gpointer) cvs;
  
  nautilus_dropbox_command_request(cvs, (DropboxCommand *) dgc);
}

static void
create_menu(NautilusDropbox *cvs, gboolean active) {
  send_for_menu(cvs, active,
		(NautilusDropboxCommandResponseHandler) get_menu_options_response_cb);
}

static void
handle_bubble(NautilusDropbox *cvs, GHashTable *args) {
  gchar *message, *caption;
  
  message = g_hash_table_lookup(args, "message");
  caption = g_hash_table_lookup(args, "caption");

  if (message != NULL && caption != NULL) {
    GError *gerr = NULL;
    GdkRectangle area;
    NotifyNotification *n =
      notify_notification_new(caption, message,
			      NULL, NULL);
    
    gtk_status_icon_get_geometry(cvs->ndt.status_icon,
				 NULL, &area, NULL);

    notify_notification_set_hint_int32 (n, "x", area.x+area.width/2);
    notify_notification_set_hint_int32 (n, "y", area.y+area.height);

    if (!notify_notification_show(n, &gerr)) {
      debug("couldn't show notification: %s", gerr->message);
      g_error_free(gerr);
    }

    g_object_unref(G_OBJECT(n));
  }
}

static void
get_dropbox_globals_cb(gchar **arr, NautilusDropbox *cvs, gpointer ud) {
  gchar *tooltip;

  if (arr == NULL && g_strv_length(arr) != 2) {
    /* TODO: we may need to do soemthing here */
    return;
  }

  tooltip = (strcmp(arr[1], "") == 0)
    ? g_strdup_printf("Dropbox %s", arr[0])
    : g_strdup_printf("Dropbox %s - %s", arr[0], arr[1]);

  gtk_status_icon_set_tooltip(cvs->ndt.status_icon, tooltip);

  g_free(tooltip);
}

static void
handle_change_to_menu(NautilusDropbox *cvs, GHashTable *args) {
  gchar *value;

  if ((value = g_hash_table_lookup(args, "active")) != NULL) {
    gboolean active;

    active = strcmp("true", value) == 0;

    create_menu(cvs, active);

    if (active != cvs->ndt.last_active) {
      nautilus_dropbox_common_get_globals(cvs, "version\temail",
					  get_dropbox_globals_cb, NULL);
      cvs->ndt.last_active = active;
    }
  }
}

static void
handle_change_state(NautilusDropbox *cvs, GHashTable *args) {
  /* great got the menu options, now build the menu and show the tray icon*/
  gchar *value;

  if ((value = g_hash_table_lookup(args, "new_state")) != NULL) {
    debug("dropbox asking us to change our state to %s", value);
    cvs->ndt.icon_state = (strcmp(value, "1") == 0);
  }
}

static void
handle_refresh_tray_menu(NautilusDropbox *cvs, GHashTable *args) {
  create_menu(cvs, cvs->ndt.last_active);
}

static void 
popup(GtkStatusIcon *status_icon,
      guint button,
      guint activate_time,
      NautilusDropbox *cvs) {
  gtk_widget_show_all(GTK_WIDGET(cvs->ndt.context_menu));

  gtk_menu_popup(GTK_MENU(cvs->ndt.context_menu),
		 NULL,
		 NULL,
		 gtk_status_icon_position_menu,
		 cvs->ndt.status_icon,
		 button,
		 activate_time);
}

static void
on_connect_menu_cb(GHashTable *response, NautilusDropbox *cvs) {
  /* this will build the initial ctx menu */
  get_menu_options_response_cb(response, cvs);

  /* get menu options / show menu */
  gtk_status_icon_set_visible(cvs->ndt.status_icon, TRUE); 
}

static void
get_active_setting_cb(gchar **arr, NautilusDropbox *cvs,
		   gpointer ud) {
  if (arr == NULL || g_strv_length(arr) != 4) {
    /* we should do something in this case */
    return;
  }

  /* convert active argument */
  cvs->ndt.last_active = strcmp(arr[0], "True") == 0;
  /* set icon state */
  cvs->ndt.icon_state = atoi(arr[3]);

  debug("our initial icon state %s", arr[3])

  /* set tooltip */
  {
    gchar *tooltip;

    tooltip = (strcmp(arr[2], "") == 0)
      ? g_strdup_printf("Dropbox %s", arr[1])
      : g_strdup_printf("Dropbox %s - %s", arr[1], arr[2]);
    
    gtk_status_icon_set_tooltip(cvs->ndt.status_icon, tooltip);

    g_free(tooltip);
  }

  /* now just set the menu and activate the toolbar */
  send_for_menu(cvs, cvs->ndt.last_active,
		(NautilusDropboxCommandResponseHandler) on_connect_menu_cb);
}

void
nautilus_dropbox_tray_on_connect(NautilusDropbox *cvs) {
  /* first get active, with version and email setting */
  nautilus_dropbox_common_get_globals(cvs, "active\tversion\temail\ticon_state",
				      get_active_setting_cb, NULL);
}

void
nautilus_dropbox_tray_on_disconnect(NautilusDropbox *cvs) {
  /* we want to hide the tray icon */
  gtk_status_icon_set_visible(cvs->ndt.status_icon, FALSE); 
}

/*
  gameplan for new tray menu:
  1. on startup start dropbox
     - if dropbox doesn't exist in client dir, then download it
     - and display download status in tray menu
  2. once dropbox starts, the connection should start and override the menu
  3. if anyone disconnects, disconnect the other one
  4. if a disconnect happens automatically, if after 5 seconds we haven't reconnected,
     kill dropboxd (if alive) and restart it
  5. if a disconenct happens because the user requested to kill dropbox, don't do that
 */

static gboolean
animate_icon(NautilusDropbox *cvs) {
  if (cvs->ndt.icon_state == 0) {
    gtk_status_icon_set_from_pixbuf(cvs->ndt.status_icon,
				    cvs->ndt.idle);
  }
  else {
    gtk_status_icon_set_from_pixbuf(cvs->ndt.status_icon,
				    cvs->ndt.busy_frame
				    ? cvs->ndt.busy
				    : cvs->ndt.busy2);
    cvs->ndt.busy_frame = !cvs->ndt.busy_frame;
  }

  g_timeout_add(500, (GSourceFunc)animate_icon, cvs);

  return FALSE;
}

void
nautilus_dropbox_tray_setup(NautilusDropbox *cvs) {
  /* register hooks from the daemon */
  g_hash_table_insert(cvs->dispatch_table, "bubble",
		      (DropboxUpdateHook) handle_bubble);
  g_hash_table_insert(cvs->dispatch_table, "change_to_menu",
		      (DropboxUpdateHook) handle_change_to_menu);
  g_hash_table_insert(cvs->dispatch_table, "change_state",
		      (DropboxUpdateHook) handle_change_state);
  g_hash_table_insert(cvs->dispatch_table, "refresh_tray_menu",
		      (DropboxUpdateHook) handle_refresh_tray_menu);

  cvs->ndt.icon_state = 0;
  cvs->ndt.busy_frame = 0;
  cvs->ndt.idle = gdk_pixbuf_new_from_inline(-1, idle, FALSE, NULL);
  cvs->ndt.busy = gdk_pixbuf_new_from_inline(-1, busy, FALSE, NULL);
  cvs->ndt.busy2 = gdk_pixbuf_new_from_inline(-1, busy2, FALSE, NULL);

  cvs->ndt.last_active = 0;
  cvs->ndt.status_icon = gtk_status_icon_new_from_pixbuf(cvs->ndt.idle);
  cvs->ndt.context_menu = NULL;
  gtk_status_icon_set_visible(cvs->ndt.status_icon, FALSE); 

  /* Connect signals */
  g_signal_connect (G_OBJECT (cvs->ndt.status_icon), "popup-menu",
		    G_CALLBACK (popup), cvs);
  
  g_signal_connect (G_OBJECT (cvs->ndt.status_icon), "activate",
		    G_CALLBACK (activate_open_my_dropbox), cvs);

  notify_init("Dropbox");

  animate_icon(cvs);
}
