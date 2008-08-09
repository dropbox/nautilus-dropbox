/*
 * Copyright 2008 Evenflow, Inc.
 *
 * nautilus-dropbox-tray.c
 * Tray icon manager code for nautilus-dropbox
 *
 * This file is part of nautilus-dropbox.
 *
 * nautilus-dropbox is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * nautilus-dropbox is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with nautilus-dropbox.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <glib-object.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <libnotify/notify.h>

#include "g-util.h"
#include "nautilus-dropbox.h"
#include "nautilus-dropbox-tray.h"
#include "nautilus-dropbox-common.h"
#include "nautilus-dropbox-command.h"
#include "nautilus-dropbox-hooks.h"
#include "async-http-downloader.h"

#include "busy2.h"
#include "busy.h"
#include "idle.h"
#include "logo.h"

typedef struct {
  NautilusDropbox *cvs;
  gchar *command;
} MenuItemData;

static void
menu_refresh(NautilusDropboxTray *ndt) {
  gboolean visible;
  
  g_object_get(G_OBJECT(ndt->context_menu), "visible", &visible, NULL);

  if (visible) {
    gtk_widget_show_all(GTK_WIDGET(ndt->context_menu));
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
send_simple_command_if_connected(NautilusDropbox *cvs,
				 const gchar *command) {
  if (nautilus_dropbox_command_is_connected(cvs) == TRUE) {
    DropboxGeneralCommand *dgc = g_new(DropboxGeneralCommand, 1);
    
    dgc->dc.request_type = GENERAL_COMMAND;
    dgc->command_name = g_strdup(command);
    dgc->command_args = NULL;
    dgc->handler = NULL;
    dgc->handler_ud = NULL;
    
    nautilus_dropbox_command_request(cvs, (DropboxCommand *) dgc);
  }
}

static void
activate_open_my_dropbox(GtkStatusIcon *status_icon,
			 NautilusDropbox *cvs) {
  send_simple_command_if_connected(cvs, "tray_action_open_dropbox");
}

static void
activate_stop_dropbox(GtkMenuItem *mi,
		      NautilusDropbox *cvs) {
  cvs->ca.user_quit = TRUE;
  send_simple_command_if_connected(cvs, "tray_action_hard_exit");
}

static void
activate_start_dropbox(GtkMenuItem *mi,
		       NautilusDropbox *cvs) {
  nautilus_dropbox_common_start_dropbox(cvs, TRUE);
}

static void
activate_menu_item(GtkStatusIcon *status_icon,
		   MenuItemData *mid) {
  send_simple_command_if_connected(mid->cvs, mid->command);
}

static void
install_start_dropbox_menu(NautilusDropbox *cvs) {
  /* install start dropbox menu */
  gtk_container_remove_all(GTK_CONTAINER(cvs->ndt.context_menu));
  
  {
    GtkWidget *item;
    item = gtk_menu_item_new_with_label("Start Dropbox");
    gtk_menu_shell_append(GTK_MENU_SHELL(cvs->ndt.context_menu), item);
    g_signal_connect(G_OBJECT(item), "activate",
		     G_CALLBACK(activate_start_dropbox), cvs);
  }
  
  menu_refresh(&(cvs->ndt));
}


void menu_item_data_destroy(MenuItemData *mid,
			    GClosure *closure) {
  g_free(mid->command);
  g_free(mid);
}

static void
build_context_menu_from_list(NautilusDropbox *cvs, 
			     GtkMenu *menu, gchar **options, gint len, int depth) {
  int i;

  /* there should be more asserts */
  g_assert(menu != NULL);
  g_assert(options != NULL);

  /* too... much... recursion... */
  if (depth > 2) {
    return;
  }

  len = len * 2;
  for (i = 0; len == -2 && options[i] != NULL || i < len; i += 2) {
    GtkWidget *item;
    
    if (strcmp(options[i], "") == 0) {
      item = gtk_separator_menu_item_new();
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }
    else {
      int num_submenu_entries;
      gchar *command;
      item = gtk_menu_item_new_with_label(options[i]);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

      command = options[i+1];

      if (strcmp(command, "") == 0) {
	g_object_set(item, "sensitive", FALSE, NULL);
      }
      else if ((num_submenu_entries = atoi(command)) != 0) {
	GtkMenu *submenu = GTK_MENU(gtk_menu_new());

	build_context_menu_from_list(cvs, submenu, &(options[i+2]),
				     num_submenu_entries, depth+1);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),
				  GTK_WIDGET(submenu));

	i += num_submenu_entries * 2;
      }
      else {
	MenuItemData *mid = g_new0(MenuItemData, 1);
	mid->cvs = cvs;
	mid->command = g_strdup(command);
	g_signal_connect_data(G_OBJECT(item), "activate",
			      G_CALLBACK(activate_menu_item), 
			      mid, (GClosureNotify) menu_item_data_destroy, 0);
      }
    }
  }
}

static void
get_menu_options_response_cb(GHashTable *response, NautilusDropbox *cvs) {
  /* great got the menu options, now build the menu and show the tray icon*/

  /*debug_enter(); */

  gchar **options;

  /* could happen if the call failed */
  if (response == NULL) {
    return;
  }

  if ((options = g_hash_table_lookup(response, "options")) != NULL &&
      g_strv_length(options) % 2 == 0) {
    gtk_container_remove_all(GTK_CONTAINER(cvs->ndt.context_menu));
    build_context_menu_from_list(cvs, cvs->ndt.context_menu, options,
				 -1, 0);
    /* now add the quit dropbox item */
    {
      GtkWidget *item;
      item = gtk_menu_item_new_with_label("Stop Dropbox");
      gtk_menu_shell_append(GTK_MENU_SHELL(cvs->ndt.context_menu), item);
      g_signal_connect(G_OBJECT(item), "activate",
		       G_CALLBACK(activate_stop_dropbox), cvs);
    }
    menu_refresh(&(cvs->ndt));
    gtk_status_icon_set_visible(cvs->ndt.status_icon, TRUE); 
  }
}

static void
create_menu(NautilusDropbox *cvs, gboolean active) {
  DropboxGeneralCommand *dgc = g_new(DropboxGeneralCommand, 1);
  
  dgc->dc.request_type = GENERAL_COMMAND;
  dgc->command_name = g_strdup("tray_action_get_menu_options");

  {
    gchar **is_active_arg;

    is_active_arg = g_new(gchar *, 2);
    is_active_arg[0] = g_strdup(active ? "true" : "false");
    is_active_arg[1] = NULL;

    dgc->command_args = g_hash_table_new_full((GHashFunc) g_str_hash,
					      (GEqualFunc) g_str_equal,
					      (GDestroyNotify) g_free,
					      (GDestroyNotify) g_strfreev);
    g_hash_table_insert(dgc->command_args,
			g_strdup("is_active"), is_active_arg);

  }
  dgc->handler = (NautilusDropboxCommandResponseHandler) get_menu_options_response_cb;
  dgc->handler_ud = (gpointer) cvs;
  
  nautilus_dropbox_command_request(cvs, (DropboxCommand *) dgc);
}

static void
is_out_of_date_cb(GHashTable *response, NautilusDropbox *cvs) {
  gchar **outofdate;

  if (response == NULL || 
      ((outofdate = g_hash_table_lookup(response, "outofdate")) != NULL &&
       strcmp(outofdate[0], "true") == 0)) {
    nautilus_dropbox_tray_bubble(&(cvs->ndt), "Out of Date",
				 "Your version of the Dropbox extension for Nautilus appears "
				 "to be out of date. It is highly recommended that you upgrade "
				 "the nautilus-dropbox package for your system.", NULL,
				 NULL, NULL, NULL);
  }
}

static void
is_out_of_date(NautilusDropbox *cvs) {
  DropboxGeneralCommand *dgc = g_new(DropboxGeneralCommand, 1);
  
  dgc->dc.request_type = GENERAL_COMMAND;
  dgc->command_name = g_strdup("is_out_of_date");

  {
    gchar **is_active_arg;

    is_active_arg = g_new(gchar *, 2);
    is_active_arg[0] = g_strdup(PACKAGE_VERSION);
    is_active_arg[1] = NULL;

    dgc->command_args = g_hash_table_new_full((GHashFunc) g_str_hash,
					      (GEqualFunc) g_str_equal,
					      (GDestroyNotify) g_free,
					      (GDestroyNotify) g_strfreev);
    g_hash_table_insert(dgc->command_args,
			g_strdup("version"), is_active_arg);

  }
  dgc->handler = (NautilusDropboxCommandResponseHandler) is_out_of_date_cb;
  dgc->handler_ud = (gpointer) cvs;
  
  nautilus_dropbox_command_request(cvs, (DropboxCommand *) dgc);
}


void notify_closed_cb(NotifyNotification *nn, gpointer ud) {
  g_object_unref(G_OBJECT(nn));
}

void notifycb(NotifyNotification *nn, gchar *label, gpointer *mud) {
  if (mud[0] != NULL) {
    ((DropboxTrayBubbleActionCB)mud[0])(mud[1]);
  }
}

void notifyfreefunc(gpointer *mud) {
  if (mud[2] != NULL) {
    ((GFreeFunc)mud[2])(mud[1]);
  }
  g_free(mud);
}

/**
 * nautilus_dropbox_tray_bubble
 * @ndt: DropboxTray structure
 * @caption: caption for the bubble
 * @message: message for the bubble
 * @cb: optional function to call when the bubble is clicked
 * @ud: optional user data for the callback
 * @free_func: optional function to call when the bubble is destroyed
 * @gerr: optional pointer to set an error
 * 
 * Bubbles a message above the Dropbox tray icon. Returns false and sets
 * gerr if an error occured.
 */
gboolean
nautilus_dropbox_tray_bubble(NautilusDropboxTray *ndt,
			     const gchar *caption,
			     const gchar *message,
			     DropboxTrayBubbleActionCB cb,
			     gpointer ud,
			     GFreeFunc free_func,
			     GError **gerr) {
  gboolean toret;
  NotifyNotification *n;
  gpointer *mud;

  /* can't do this if libnotify couldn't initialize */
  if (!ndt->notify_inited) {
    return TRUE;
  }
  
#if GTK_CHECK_VERSION(2, 9, 2)
  n = notify_notification_new_with_status_icon(caption, message,
					       NULL, ndt->status_icon);
#else
  n = notify_notification_new(caption, message,
			      NULL, ndt->status_icon);
  {
      GdkRectangle area;
      gtk_status_icon_get_geometry(ndt->status_icon,
				   NULL, &area, NULL);
      
      notify_notification_set_hint_int32 (n, "x", area.x+area.width/2);
      notify_notification_set_hint_int32 (n, "y", area.y+area.height - 5);
  }
#endif

  mud = g_new(gpointer, 3);
  mud[0] = cb;
  mud[1] = ud;
  mud[2] = free_func;
  notify_notification_add_action(n, "default", "Show Changed Files",
				 (NotifyActionCallback) notifycb, mud,
				 (GFreeFunc) notifyfreefunc);

  g_signal_connect(n, "closed", G_CALLBACK(notify_closed_cb), NULL);

  if ((toret = notify_notification_show(n, gerr)) == FALSE) {
    debug("couldn't show notification: %s", (*gerr)->message);
    g_free(mud);
  }

  return toret;
}

void bubble_clicked_cb(gpointer *ud) {
  debug_enter();

  if (g_file_test((const gchar *) ud[1], G_FILE_TEST_EXISTS) == FALSE) {
    return;
  }
  
  if (g_file_test((const gchar *) ud[1], G_FILE_TEST_IS_DIR) == FALSE) {
    gchar *base;
    base = g_path_get_dirname(ud[1]);
    nautilus_dropbox_common_launch_folder((NautilusDropbox *) ud[0],
					  (const gchar *) base);
    g_free(base);
  }
  else {
    nautilus_dropbox_common_launch_folder((NautilusDropbox *) ud[0],
					  (const gchar *) ud[1]);
  }

}

void bubble_clicked_free(gpointer *ud) {
  g_free(ud[1]);
  g_free(ud);
}

static void
handle_bubble(GHashTable *args, NautilusDropbox *cvs) {
  gchar **message, **caption;
  
  message = g_hash_table_lookup(args, "message");
  caption = g_hash_table_lookup(args, "caption");

  if (message != NULL && caption != NULL) {
    gchar **location;

    location = g_hash_table_lookup(args, "location");
    if (location != NULL) {
      gpointer *ud;
      ud = g_new(gpointer, 2);
      ud[0] = cvs;
      ud[1] = g_strdup(location[0]);
      nautilus_dropbox_tray_bubble(&(cvs->ndt), caption[0], message[0],
				   (DropboxTrayBubbleActionCB) bubble_clicked_cb, ud,
				   (GFreeFunc) bubble_clicked_free, NULL);      
    }
    else {
      nautilus_dropbox_tray_bubble(&(cvs->ndt), caption[0], message[0],
				   NULL,NULL,NULL,NULL);      
    }
  }
}

static void
get_dropbox_globals_cb(gchar **arr, NautilusDropbox *cvs, gpointer ud) {
  gchar *tooltip;

  if (arr == NULL && g_strv_length(arr) != 2) {
    /* TODO: we may need to do soemthing here,
       probably restart dropbox */
    return;
  }

  tooltip = (strcmp(arr[1], "") == 0)
    ? g_strdup_printf("Dropbox %s", arr[0])
    : g_strdup_printf("Dropbox %s - %s", arr[0], arr[1]);

  gtk_status_icon_set_tooltip(cvs->ndt.status_icon, tooltip);

  g_free(tooltip);
}

static void
handle_change_to_menu(GHashTable *args, NautilusDropbox *cvs) {
  gchar **value;

  if ((value = g_hash_table_lookup(args, "active")) != NULL) {
    gboolean active;

    active = strcmp("true", value[0]) == 0;

    create_menu(cvs, active);

    if (active != cvs->ndt.last_active) {
      nautilus_dropbox_common_get_globals(cvs, "version\temail",
					  get_dropbox_globals_cb, NULL);
      cvs->ndt.last_active = active;
    }
  }
}

static void
set_icon(NautilusDropboxTray *ndt) {
  GdkPixbuf *toset = NULL;

  switch (ndt->icon_state) {
  case NOT_CONNECTED:
    toset = ndt->logo;
    break;
  case SYNCING:
    toset = ndt->busy_frame ? ndt->busy : ndt->busy2;
    break;
  case UPTODATE:
    toset = ndt->idle;
    break;
  default:
    g_assert_not_reached();
    /* but fail transparently on production */
    toset = ndt->logo;
    break;
  }

  gtk_status_icon_set_from_pixbuf(ndt->status_icon, toset);
}

static void
handle_change_state(GHashTable *args, NautilusDropbox *cvs) {
  /* great got the menu options, now build the menu and show the tray icon*/
  gchar **value;

  if ((value = g_hash_table_lookup(args, "new_state")) != NULL) {
    /*    debug("dropbox asking us to change our state to %s", value[0]);*/
    cvs->ndt.icon_state = atoi(value[0]);
    set_icon(&(cvs->ndt));
  }
}

static void
handle_refresh_tray_menu(GHashTable *args, NautilusDropbox *cvs) {
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

  /* set icon state, 0 means IDLE, 1 means BUSY, 2 means CONNECTING*/
  cvs->ndt.icon_state = atoi(arr[3]);
  set_icon(&(cvs->ndt));

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

  is_out_of_date(cvs);
}

void
nautilus_dropbox_tray_on_connect(NautilusDropbox *cvs) {
  /* first get active, with version and email setting */
  nautilus_dropbox_common_get_globals(cvs, "active\tversion\temail\ticon_state",
				      get_active_setting_cb, NULL);
}

void
nautilus_dropbox_tray_on_disconnect(NautilusDropbox *cvs) {
  if (cvs->ca.user_quit == TRUE) {
    install_start_dropbox_menu(cvs);
    gtk_status_icon_set_tooltip(cvs->ndt.status_icon, "Dropbox");
  }
  else {
    gtk_container_remove_all(GTK_CONTAINER(cvs->ndt.context_menu));
    GtkWidget *item;
    item = gtk_menu_item_new_with_label("Reconnecting to Dropbox...");
    gtk_menu_shell_append(GTK_MENU_SHELL(cvs->ndt.context_menu), item);
    g_object_set(item, "sensitive", FALSE, NULL);    
    menu_refresh(&(cvs->ndt));
    gtk_status_icon_set_tooltip(cvs->ndt.status_icon, "Reconnecting to Dropbox...");
  }
  
  cvs->ndt.icon_state = NOT_CONNECTED;
  set_icon(&(cvs->ndt));
}

typedef struct {
  NautilusDropbox *cvs;
  GtkLabel *percent_done_label;
  gint filesize;
  GIOChannel *tmpfilechan;
  gchar *tmpfilename;
  gint bytes_downloaded;
  guint ev_id;
  gboolean user_cancelled;
  gboolean download_finished;
} HttpDownloadCtx;

static void
fail_dropbox_download(NautilusDropbox *cvs, const gchar *msg) {
  install_start_dropbox_menu(cvs);

  nautilus_dropbox_tray_bubble(&(cvs->ndt), "Couldn't download Dropbox",
			       msg == NULL
			       ? "Failed to download Dropbox, Are you connected "
			       " to the internet? Are your proxy settings correct?"
			       : msg, NULL, NULL, NULL, NULL);
}

static void
handle_tar_dying(GPid pid, gint status, gpointer *ud) {
  if (status == 0) {
    g_unlink(ud[0]);
    nautilus_dropbox_common_start_dropbox(ud[1], FALSE);
  }
  else {
    gchar *msg;
    msg = g_strdup_printf("The Dropbox archive located at \"%s\" failed to unpack.",
			  (gchar *) ud[0]);
    nautilus_dropbox_tray_bubble(&(((NautilusDropbox *) ud[1])->ndt),
				 "Couldn't download Dropbox.",
				 msg, NULL, NULL, NULL, NULL);
    g_free(msg);
  }

  /* delete tmp file */
  g_spawn_close_pid(pid);
  g_free(ud[0]);
  g_free(ud);
}

static gboolean
handle_incoming_http_data(GIOChannel *chan,
			  GIOCondition cond,
			  HttpDownloadCtx *ctx) {
  GIOStatus iostat;
  gchar buf[4096];
  gsize bytes_read;
  while ((iostat = g_io_channel_read_chars(chan, buf, 4096,
					   &bytes_read, NULL)) ==
	 G_IO_STATUS_NORMAL) {
    ctx->bytes_downloaded += bytes_read;
    
    /* TODO: this blocks, should put buffesr to write on a queue
       that gets read whenever ctx->tmpfilechan is ready to write,
       we should be okay for now since it shouldn't block except
       only in EXTREME circumstances */
    if (g_io_channel_write_chars(ctx->tmpfilechan, buf,
				 bytes_read, NULL, NULL) != G_IO_STATUS_NORMAL) {
      /* TODO: error condition, ignore for now */
    }
  }
  
  /* now update the gtk label */
  if (ctx->filesize != -1) {
    gchar *percent_done;
    
    percent_done =
      g_strdup_printf("Downloading Dropbox... %d%% Done",
		      ctx->bytes_downloaded * 100 / ctx->filesize);
    
    gtk_label_set_text(GTK_LABEL(ctx->percent_done_label), percent_done);
    g_free(percent_done);
  }
  else {
    switch (ctx->bytes_downloaded % 4) {
    case 0:
      gtk_label_set_text(GTK_LABEL(ctx->percent_done_label), "Downloading Dropbox");
      break;
    case 1:
      gtk_label_set_text(GTK_LABEL(ctx->percent_done_label), "Downloading Dropbox.");
      break;
    case 2:
      gtk_label_set_text(GTK_LABEL(ctx->percent_done_label), "Downloading Dropbox..");
      break;
    default:
      gtk_label_set_text(GTK_LABEL(ctx->percent_done_label), "Downloading Dropbox...");
      break;
    }
  }
  
  switch (iostat) {
  case G_IO_STATUS_EOF: {
    GPid pid;
    gchar **argv;
    /* completed download, untar the archive and run */
    ctx->download_finished = TRUE;
    
    argv = g_new(gchar *, 6);
    argv[0] = g_strdup("tar");
    argv[1] = g_strdup("-C");
    argv[2] = g_strdup(g_get_home_dir());
    argv[3] = g_strdup("-xjf");
    argv[4] = g_strdup(ctx->tmpfilename);
    argv[5] = NULL;
    
    g_spawn_async(NULL, argv, NULL,
		  G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
		  NULL, NULL, &pid, NULL);
    g_strfreev(argv);
    
    {
      gpointer *ud2;
      ud2 = g_new(gpointer, 2);
      ud2[0] = g_strdup(ctx->tmpfilename);
      ud2[1] = ctx->cvs;
      g_child_watch_add(pid, (GChildWatchFunc) handle_tar_dying, ud2);
    }
    
    return FALSE;
  }
    break;
  case G_IO_STATUS_ERROR: {
    /* just an error, return false to stop the download without setting download
       finished*/
    return FALSE;
  }
    break;
  case G_IO_STATUS_AGAIN:
    return TRUE;
    break;
  default:
    g_assert_not_reached();
    return FALSE;      
    break;
  }
}

static void
kill_hihd_ud(HttpDownloadCtx *ctx) {
  if (ctx->user_cancelled == TRUE) {
    install_start_dropbox_menu(ctx->cvs);
  }
  else if (ctx->download_finished == FALSE) {
    fail_dropbox_download(ctx->cvs, NULL);
  }

  gtk_status_icon_set_tooltip(ctx->cvs->ndt.status_icon, "Dropbox");

  g_io_channel_unref(ctx->tmpfilechan);
  g_free(ctx->tmpfilename);
  g_free(ctx);
}

static void
activate_cancel_download(GtkMenuItem *mi,
			 HttpDownloadCtx *ctx) {
  ctx->user_cancelled = TRUE;
  g_source_remove(ctx->ev_id);
}

static void
handle_dropbox_download_response(gint response_code,
				 GList *headers,
				 GIOChannel *chan,
				 HttpDownloadCtx *ctx) {
  int filesize = -1;

  switch (response_code) {
  case -1:
    fail_dropbox_download(ctx->cvs, NULL);
    g_free(ctx);
    return;
    break;
  case 200:
    break;
  default: {
    gchar *msg;
    
    msg = g_strdup_printf("Couldn't download Dropbox. Server returned "
			  "%d.", response_code);
    fail_dropbox_download(ctx->cvs, msg);
    g_free(msg);
    g_free(ctx);
    return;
  }
    break;
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
    if (g_io_channel_set_flags(chan, flags | G_IO_FLAG_NONBLOCK, NULL) !=
	G_IO_STATUS_NORMAL) {
      fail_dropbox_download(ctx->cvs, NULL);
      g_free(ctx);
      return;
    }
  }

  if (g_io_channel_set_encoding(chan, NULL, NULL) != G_IO_STATUS_NORMAL) {
    fail_dropbox_download(ctx->cvs, NULL);
    g_free(ctx);
    return;
  }

  ctx->filesize = filesize;
  {
    gint fd;
    gchar *filename;
    fd = g_file_open_tmp(NULL, &filename, NULL);
    if (fd == -1) {
      fail_dropbox_download(ctx->cvs, NULL);
      g_free(ctx);
      return;
    }

    /*debug("saving to %s", filename); */

    ctx->tmpfilechan = g_io_channel_unix_new(fd);
    g_io_channel_set_close_on_unref(ctx->tmpfilechan, TRUE);

    if (g_io_channel_set_encoding(ctx->tmpfilechan, NULL, NULL) != G_IO_STATUS_NORMAL) {
      fail_dropbox_download(ctx->cvs, NULL);
      g_io_channel_unref(ctx->tmpfilechan);
      g_free(filename);
      g_free(ctx);
      return;
    }

    ctx->tmpfilename = filename;
  }

  /*debug("installing http receiver callback"); */

  ctx->user_cancelled = FALSE;
  ctx->bytes_downloaded = 0;
  ctx->download_finished = FALSE;
  ctx->ev_id = g_io_add_watch_full(chan, G_PRIORITY_DEFAULT,
				   G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
				   (GIOFunc) handle_incoming_http_data,
				   ctx, (GDestroyNotify) kill_hihd_ud);

  /* great we got here, now set downloading menu */
  {
    /* setup the menu here */
    gtk_container_remove_all(GTK_CONTAINER(ctx->cvs->ndt.context_menu));
    
    {
      GtkWidget *item;
      item = gtk_menu_item_new_with_label("Downloading Dropbox...");
      ctx->percent_done_label = GTK_LABEL(gtk_bin_get_child(GTK_BIN(item)));
      
      g_object_set(item, "sensitive", FALSE, NULL);
      gtk_menu_shell_append(GTK_MENU_SHELL(ctx->cvs->ndt.context_menu), item);
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(ctx->cvs->ndt.context_menu), gtk_separator_menu_item_new());

    {
      GtkWidget *item;
      item = gtk_menu_item_new_with_label("Cancel Download");
      
      gtk_menu_shell_append(GTK_MENU_SHELL(ctx->cvs->ndt.context_menu), item);

      g_signal_connect(G_OBJECT(item), "activate",
		       G_CALLBACK(activate_cancel_download), ctx);
    }

    menu_refresh(&(ctx->cvs->ndt));
  }
}

void
nautilus_dropbox_tray_start_dropbox_transfer(NautilusDropbox *cvs) {
  /* setup the menu here */
  gtk_container_remove_all(GTK_CONTAINER(cvs->ndt.context_menu));
  {
    GtkWidget *item;
    item = gtk_menu_item_new_with_label("Attempting to download Dropbox...");
    g_object_set(item, "sensitive", FALSE, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(cvs->ndt.context_menu), item);
  }
  menu_refresh(&(cvs->ndt));

  gtk_status_icon_set_tooltip(cvs->ndt.status_icon, "Downloading Dropbox...");
  
  {
    gchar *dropbox_platform, *webpath;
    HttpDownloadCtx *ctx;
    ctx = g_new(HttpDownloadCtx, 1);
    ctx->cvs = cvs;
    
    dropbox_platform = nautilus_dropbox_common_get_platform();
    webpath = g_strdup_printf("/u/5143/dropbox-%s-latest.tar.bz2", dropbox_platform);
    
    if (make_async_http_get_request("dl.getdropbox.com", webpath,
				    NULL, (HttpResponseHandler) handle_dropbox_download_response,
				    (gpointer) ctx) == FALSE) {
      fail_dropbox_download(cvs, NULL);
    }
    
    g_free(dropbox_platform);
    g_free(webpath);
  }
}

/* TODO: needs cleanup, notion of icon_state separate from animation frame */
static gboolean
animate_icon(NautilusDropboxTray *ndt) {
  if (ndt->icon_state == SYNCING) {
    ndt->busy_frame = !ndt->busy_frame;
    set_icon(ndt);
  }

  g_timeout_add(500, (GSourceFunc)animate_icon, ndt);

  return FALSE;
}

void
nautilus_dropbox_tray_setup(NautilusDropbox *cvs) {
  /* register hooks from the daemon */
  nautilus_dropbox_hooks_add(&(cvs->hookserv), "bubble",
			     (DropboxUpdateHook) handle_bubble, cvs);
  nautilus_dropbox_hooks_add(&(cvs->hookserv), "change_to_menu",
			     (DropboxUpdateHook) handle_change_to_menu, cvs);
  nautilus_dropbox_hooks_add(&(cvs->hookserv), "change_state",
			     (DropboxUpdateHook) handle_change_state, cvs);
  nautilus_dropbox_hooks_add(&(cvs->hookserv), "refresh_tray_menu",
			     (DropboxUpdateHook) handle_refresh_tray_menu, cvs);

  cvs->ndt.icon_state = NOT_CONNECTED;
  cvs->ndt.busy_frame = 0;
  cvs->ndt.idle = gdk_pixbuf_new_from_inline(-1, idle, FALSE, NULL);
  cvs->ndt.busy = gdk_pixbuf_new_from_inline(-1, busy, FALSE, NULL);
  cvs->ndt.busy2 = gdk_pixbuf_new_from_inline(-1, busy2, FALSE, NULL);
  cvs->ndt.logo = gdk_pixbuf_new_from_inline(-1, logo, FALSE, NULL);

  cvs->ndt.last_active = 0;
  cvs->ndt.status_icon = gtk_status_icon_new_from_pixbuf(cvs->ndt.logo);
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

  /* TODO: do a popup notification if this failed */
  cvs->ndt.notify_inited = notify_init("Dropbox");

  animate_icon(&(cvs->ndt));
}
