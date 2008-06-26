/*
 *
 *  nautilus-dropbox-common.c util functions
 * 
 */

#ifdef HAVE_CONFIG_H
#include <config.h> /* for GETTEXT_PACKAGE */
#endif

#include <glib.h>
#include <gdk/gdk.h>

#include "nautilus-dropbox-common.h"
#include "nautilus-dropbox-command.h"

static void nullfunc(gpointer ud) {
}

gboolean
nautilus_dropbox_common_execute_command_line(const gchar *command_line) {
  gint argc;
  gchar **argv;
  
  g_shell_parse_argv(command_line, &argc, &argv, NULL);
  
  return gdk_spawn_on_screen(gdk_screen_get_default(),
			     NULL, argv, NULL,
			     G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
			     G_SPAWN_STDERR_TO_DEV_NULL,
			     nullfunc, NULL,
			     NULL, NULL);
}

void
nautilus_dropbox_common_destroy_string(gpointer data) {
  g_string_free((GString *) data, TRUE);
  return;
}

typedef struct {
  NautilusDropbox *cvs;
  NautilusDropboxGlobalCB cb;
  gpointer ud;
} GetGlobalsData;

static void
get_dropbox_globals_cb(GHashTable *response, GetGlobalsData *ggd) {
  GString *key, *value;
  
  key = g_string_new("values");
  
  if (response != NULL &&
      (value = g_hash_table_lookup(response, key)) != NULL) {
    gchar **value_arr;

    value_arr = g_strsplit(value->str, "\t", 0);
    ggd->cb(value_arr, ggd->cvs, ggd->ud);
    g_strfreev(value_arr);
  }
  else {
    ggd->cb(NULL, ggd->cvs, ggd->ud);
  }

  g_free(ggd);
  g_string_free(key, TRUE);
}

void
nautilus_dropbox_common_get_globals(NautilusDropbox *cvs,
				    gchar *tabbed_keys,
				    NautilusDropboxGlobalCB cb, gpointer ud) {
  DropboxGeneralCommand *dgc;

  g_assert(cvs != NULL);
  g_assert(cb != NULL);
  g_assert(tabbed_keys != NULL);

  dgc = g_new(DropboxGeneralCommand, 1);
  
  dgc->dc.request_type = GENERAL_COMMAND;
  dgc->command_name = g_strdup("get_dropbox_globals");

  /* build command args */
  {
    dgc->command_args = g_hash_table_new_full((GHashFunc) g_string_hash,
					      (GEqualFunc) g_string_equal,
					      nautilus_dropbox_common_destroy_string,
					      nautilus_dropbox_common_destroy_string);
    g_hash_table_insert(dgc->command_args,
			g_string_new("keys"), g_string_new(tabbed_keys));
  }
  
  dgc->handler = (NautilusDropboxCommandResponseHandler) get_dropbox_globals_cb;
  {
    GetGlobalsData *ggd;
    ggd = g_new0(GetGlobalsData, 1);
    ggd->cvs = cvs;
    ggd->cb = cb;
    ggd->ud = ud;
    dgc->handler_ud = (gpointer) ggd;
  }

  nautilus_dropbox_command_request(cvs, (DropboxCommand *) dgc);
}

/* TODO: return something that indicates status of call */
void
nautilus_dropbox_common_start_dropbox(NautilusDropbox *cvs, gboolean download) {
  gchar *dropboxd_path;

  /* first kill any pre-existing instances of dropbox */
  /* TODO: is there a portable way to kill processes by name? */
  g_spawn_command_line_async("killall dropboxd", NULL);

  /* now check if dropboxd exists in the magic place,
     first generate executable name
   */
  dropboxd_path = g_strdup_printf("%s/.dropbox-dist/dropboxd",
                                  g_get_home_dir());

  /* if dropboxd exists let's just start it */
  if (g_file_test(dropboxd_path,
                  G_FILE_TEST_IS_EXECUTABLE | G_FILE_TEST_EXISTS)) {
    /* if there was a problem with starting dropbox
       something's up with the package, we should redownload it */
    /* TODO: this should daemonize dropbox,
       i.e. remove the controlling tty etc */
    if (g_spawn_command_line_async(dropboxd_path, NULL)) {
      goto OUTTRUE;
    }
    else {
      gchar *delete_dropbox_cmdline;
      
      g_free(dropboxd_path);
      delete_dropbox_cmdline = g_strdup_printf("rm -rf %s/.dropbox-dist/",
                                               g_get_home_dir());
      g_spawn_command_line_async(delete_dropbox_cmdline, NULL);
      
      g_free(delete_dropbox_cmdline);

      if (download) {
	nautilus_dropbox_tray_start_dropbox_transfer(cvs);
	goto OUTTRUE;
      }
      else {
	goto OUTFALSE;
      }
    }
  }
  /* else we have to download it */
  else {
    if (download) {
      nautilus_dropbox_tray_start_dropbox_transfer(cvs);
      goto OUTTRUE;
    }
    else {
      goto OUTFALSE;
    }
  }

 OUTTRUE:
  g_free(dropboxd_path);
  return TRUE;

 OUTFALSE:
  g_free(dropboxd_path);
  return FALSE;
}
