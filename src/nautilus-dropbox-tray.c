/*
 *
 *  nautilus-dropbox-tray.c
 *
 */

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <glib-object.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <libnotify/notify.h>

#include "nautilus-dropbox.h"
#include "nautilus-dropbox-common.h"
#include "nautilus-dropbox-command.h"
#include "nautilus-dropbox-hooks.h"
#include "async-http-downloader.h"

#include "busy2.h"
#include "busy.h"
#include "idle.h"

typedef struct {
  NautilusDropbox *cvs;
  gchar *command;
} MenuItemData;

static void
menu_refresh(NautilusDropbox *cvs) {
  gboolean visible;
  
  gtk_object_get(cvs->ndt.context_menu, "visible", &visible, NULL);

  if (visible) {
    gtk_widget_show_all(GTK_WIDGET(cvs->ndt.context_menu));
  }
}

static void
gtk_container_remove_all(GtkContainer *c) {
  GList *li;
  
  for (li = gtk_container_get_children(c); li != NULL;
       li = g_list_next(li)) {
    gtk_container_remove(c, li->data);
  }
}

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
activate_stop_dropbox(GtkStatusIcon *status_icon,
		      NautilusDropbox *cvs) {
  if (nautilus_dropbox_command_is_connected(cvs) == TRUE) {
    DropboxGeneralCommand *dgc;

    cvs->ca.user_quit = TRUE;

    dgc = g_new(DropboxGeneralCommand, 1);
    
    dgc->dc.request_type = GENERAL_COMMAND;
    dgc->command_name = g_strdup("tray_action_hard_exit");
    dgc->command_args = NULL;
    dgc->handler = NULL;
    dgc->handler_ud = NULL;
    
    nautilus_dropbox_command_request(cvs, (DropboxCommand *) dgc);
  }
}

static void
activate_start_dropbox(GtkStatusIcon *status_icon,
		       NautilusDropbox *cvs) {
  nautilus_dropbox_common_start_dropbox(cvs, TRUE);
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
    item = gtk_menu_item_new_with_label("Stop Dropbox");
    gtk_menu_shell_append(GTK_MENU_SHELL(cvs->ndt.context_menu), item);
    g_signal_connect(G_OBJECT(item), "activate",
		     G_CALLBACK(activate_stop_dropbox), cvs);
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
    /* TODO: don't kill old menu here, wait until next popup */
    gtk_container_remove_all(GTK_CONTAINER(cvs->ndt.context_menu));
    build_context_menu_from_list(cvs, options_arr);
    menu_refresh(cvs);
    gtk_status_icon_set_visible(cvs->ndt.status_icon, TRUE); 
  }

  g_string_free(options_key, TRUE);
}

static void
create_menu(NautilusDropbox *cvs, gboolean active) {
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

  debug("our initial icon state %s", arr[3]);

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
  create_menu(cvs, cvs->ndt.last_active);
}

void
nautilus_dropbox_tray_on_connect(NautilusDropbox *cvs) {
  /* first get active, with version and email setting */
  nautilus_dropbox_common_get_globals(cvs, "active\tversion\temail\ticon_state",
				      get_active_setting_cb, NULL);
}

void
nautilus_dropbox_tray_on_disconnect(NautilusDropbox *cvs) {
  /* TODO: change menu to start_dropbox */

  /* setup the menu here */
  gtk_container_remove_all(GTK_CONTAINER(cvs->ndt.context_menu));
  
  if (cvs->ca.user_quit == TRUE) {
    GtkWidget *item;
    item = gtk_menu_item_new_with_label("Start Dropbox");
    gtk_menu_shell_append(GTK_MENU_SHELL(cvs->ndt.context_menu), item);

    g_signal_connect(G_OBJECT(item), "activate",
		     G_CALLBACK(activate_start_dropbox), cvs);
  }
  else {
    GtkWidget *item;
    item = gtk_menu_item_new_with_label("Reconnecting to Dropbox...");
    gtk_menu_shell_append(GTK_MENU_SHELL(cvs->ndt.context_menu), item);
    g_object_set(item, "sensitive", FALSE, NULL);    
  }
  
  menu_refresh(cvs);
}

static void
handle_tar_dying(GPid pid, gint status, gpointer *ud) {
  /* TODO: probably check status */
  nautilus_dropbox_common_start_dropbox(ud[1], FALSE);
  /* delete tmp file */
  g_unlink(ud[0]);
  g_spawn_close_pid(pid);
  g_free(ud[0]);
  g_free(ud);
}

static gboolean
handle_incoming_http_data(GIOChannel *chan,
			  GIOCondition cond,
			  gpointer data) {
  gpointer *ud = data;
  
  g_assert((cond & G_IO_IN) || (cond & G_IO_PRI));
  
  {
    GIOStatus iostat;
    gchar buf[4096];
    gsize bytes_read;
    while ((iostat = g_io_channel_read_chars(chan, buf, 4096,
					     &bytes_read, NULL)) ==
	   G_IO_STATUS_NORMAL) {
      ud[5] = GINT_TO_POINTER(GPOINTER_TO_INT(ud[5]) + bytes_read);

      /* TODO: this blocks, should put buffesr to write on a queue
	 that gets read whenever ud[3] is ready to write,
	 we should be okay for now since it shouldn't block except
	 only in EXTREME circumstances */
      if (g_io_channel_write_chars(ud[3], buf,
				   bytes_read, NULL, NULL) != G_IO_STATUS_NORMAL) {
	/* TODO: error condition, ignore for now */
      }
    }

    /* now update the gtk label */
    if (GPOINTER_TO_INT(ud[2]) != -1) {
      gchar *percent_done;
      
      percent_done =
	g_strdup_printf("Downloading Dropbox... %d%% Done",
			GPOINTER_TO_INT(ud[5]) * 100 / GPOINTER_TO_INT(ud[2]));
      
      gtk_label_set_text(GTK_LABEL(ud[1]), percent_done);
      g_free(percent_done);
    }
    
    if (iostat == G_IO_STATUS_EOF) {
      GPid pid;
      gchar **argv;
      /* completed download, untar the archive and run */
      argv = g_new(gchar *, 6);
      argv[0] = g_strdup("tar");
      argv[1] = g_strdup("-C");
      argv[2] = g_strdup(g_get_home_dir());
      argv[3] = g_strdup("-xzf");
      argv[4] = g_strdup(ud[4]);
      argv[5] = NULL;

      g_spawn_async(NULL, argv, NULL,
		    G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
		    NULL, NULL, &pid, NULL);
      g_strfreev(argv);
      
      {
	gpointer *ud2;
	ud2 = g_new(gpointer, 2);
	ud2[0] = g_strdup(ud[4]);
	ud2[1] = ud[0];
	g_child_watch_add(pid, (GChildWatchFunc) handle_tar_dying, ud2);
      }
      
      return FALSE;
    }
    else {
      return TRUE;
    }
  }
}

static void
kill_hihd_ud(gpointer data) {
  gpointer *ud = (gpointer *) data;

  g_io_channel_unref(ud[3]);
  g_free(ud[4]);
  g_free(ud);
}

static void
handle_dropbox_download_response(gint response_code,
				 GList *headers,
				 GIOChannel *chan,
				 gpointer *ud) {
  debug_enter();

  int filesize = -1;

  /* TODO: do something when http request fails, but what?
     show issue in context menu? dropbox is unstartable? 
     maybe try again in some amount of time
  */
  if (response_code != 200) {
    debug("downloading dropbox dist failed with %d", response_code);
    g_free(ud);
    return;
  }

  /* find out the file size, -1 if unknown */
  {
    GList *li;
    for (li = headers; li != NULL; li = g_list_next(li)) {
      if (strncasecmp("content-length:", li->data, sizeof("content-length:")-1) == 0) {
	char *number_loc = strchr(li->data, ':');
	if (number_loc != NULL) {
	  filesize = atoi(number_loc + 1);
	  break;
	}
      }
    }
  }

  /* set channel to non blocking */
  {
    GIOFlags flags;
    flags = g_io_channel_get_flags(chan);
    /* TODO: error fail otu for now */
    if (g_io_channel_set_flags(chan, flags | G_IO_FLAG_NONBLOCK, NULL) !=
	G_IO_STATUS_NORMAL) {
      g_io_channel_unref(chan);
      g_free(ud);
      return;
    }
  }

  if (g_io_channel_set_encoding(chan, NULL, NULL) != G_IO_STATUS_NORMAL) {
    /* TODO: error fail out for now */
    g_free(ud);
    return;
  }

  ud[2] = GINT_TO_POINTER(filesize);
  {
    gint fd;
    gchar *filename;
    fd = g_file_open_tmp(NULL, &filename, NULL);
    /* TODO: error fail out for now*/
    if (fd == -1) {
      g_free(ud);
      return;
    }

    debug("saving to %s", filename);

    ud[3] = g_io_channel_unix_new(fd);
    g_io_channel_set_close_on_unref(ud[3], TRUE);

    /* TODO: error fail out for now */
    if (g_io_channel_set_encoding(ud[3], NULL, NULL) != G_IO_STATUS_NORMAL) {
      g_io_channel_unref(ud[3]);
      g_free(filename);
      g_free(ud);
      return;
    }

    ud[4] = filename;
  }

  debug("installing http receiver callback");

  ud[5] = GINT_TO_POINTER(0);
  g_io_add_watch_full(chan, G_PRIORITY_DEFAULT,
		      G_IO_IN | G_IO_PRI,
		      handle_incoming_http_data,
		      ud, kill_hihd_ud);
}

void
nautilus_dropbox_tray_start_dropbox_transfer(NautilusDropbox *cvs) {
  GtkLabel *percent_done_label;

  /* setup the menu here */
  gtk_container_remove_all(GTK_CONTAINER(cvs->ndt.context_menu));

  {
    GtkWidget *item;
    gchar *percent_done;

    percent_done = g_strdup_printf("Downloading Dropbox... %d%% Done", 0);

    item = gtk_menu_item_new_with_label(percent_done);
    g_free(percent_done);

    percent_done_label = GTK_LABEL(gtk_bin_get_child(GTK_BIN(item)));

    g_object_set(item, "sensitive", FALSE, NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(cvs->ndt.context_menu), item);
  }

  gtk_menu_shell_append(GTK_MENU_SHELL(cvs->ndt.context_menu),
			gtk_separator_menu_item_new());
  
  {
    GtkWidget *item;

    item = gtk_menu_item_new_with_label("Cancel Download");
    /* TODO: add activation signal */
    gtk_menu_shell_append(GTK_MENU_SHELL(cvs->ndt.context_menu), item);
  }

  {
    gpointer *ud;
    /* TODO: this one little data structure deals with
       this entire transfer, a little unclean */
    ud = g_new(gpointer, 6);
    ud[0] = (gpointer) cvs;
    ud[1] = (gpointer) percent_done_label;
    debug("making request for http://%s%s...",
	  "sunkenship",
	  "/~rian/dropbox-linux-ubuntu-8.04-amd64.tar.gz");

    if (make_async_http_get_request(
				    //"dl.getdropbox.com", "/u/5143/dropbox-linux-amd64.tar.gz",
				    //"sunkenship", "/~rian/dropbox-linux-ubuntu-8.04-amd64.tar.gz",
				    "foursquare.nfshost.com", "/bb.tar.gz",
				    NULL, (HttpResponseHandler) handle_dropbox_download_response,
				    (gpointer) ud) == FALSE) {
      /* TODO: on failure? */
    }
  }
  
  menu_refresh(cvs);
}

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
  {
    GtkWidget *item;
    cvs->ndt.context_menu = GTK_MENU(gtk_menu_new());
    item = gtk_menu_item_new_with_label("Connecting to Dropbox...");
    gtk_menu_shell_append(GTK_MENU_SHELL(cvs->ndt.context_menu), item);
    g_object_set(item, "sensitive", FALSE, NULL);    
  }

  gtk_status_icon_set_visible(cvs->ndt.status_icon, TRUE); 

  /* Connect signals */
  g_signal_connect (G_OBJECT (cvs->ndt.status_icon), "popup-menu",
		    G_CALLBACK (popup), cvs);
  
  g_signal_connect (G_OBJECT (cvs->ndt.status_icon), "activate",
		    G_CALLBACK (activate_open_my_dropbox), cvs);

  notify_init("Dropbox");

  animate_icon(cvs);
}
