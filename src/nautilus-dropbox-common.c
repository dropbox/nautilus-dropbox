/*
 *
 *  nautilus-dropbox-common.c nautilus-dropbox dependent util functions
 * 
 */

#include <unistd.h>

#ifdef HAVE_CONFIG_H
#include <config.h> /* for GETTEXT_PACKAGE */
#endif

#include <glib.h>
#include <gdk/gdk.h>

#include "g-util.h"
#include "nautilus-dropbox-common.h"
#include "nautilus-dropbox-command.h"
#include "nautilus-dropbox-tray.h"

typedef struct {
  NautilusDropbox *cvs;
  NautilusDropboxGlobalCB cb;
  gpointer ud;
} GetGlobalsData;

static void
get_dropbox_globals_cb(GHashTable *response, GetGlobalsData *ggd) {
  gchar **value;
  
  if (response != NULL &&
      (value = g_hash_table_lookup(response, "values")) != NULL) {
    ggd->cb(value, ggd->cvs, ggd->ud);
  }
  else {
    ggd->cb(NULL, ggd->cvs, ggd->ud);
  }
  
  g_free(ggd);
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
    dgc->command_args = g_hash_table_new_full((GHashFunc) g_str_hash,
					      (GEqualFunc) g_str_equal,
					      (GDestroyNotify) g_free,
					      (GDestroyNotify) g_strfreev);

    g_hash_table_insert(dgc->command_args, g_strdup("keys"),
			g_strsplit(tabbed_keys, "\t", 0));
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

static 
void dropboxd_setup(gpointer ud) {
  /* daemonize dropbox */
  setsid();
}

gboolean
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
    gchar **argv;

    argv = g_new(gchar *, 2);
    argv[0] = dropboxd_path;
    argv[1] = NULL;
    
    if (g_spawn_async(g_get_home_dir(), argv, NULL, 0, dropboxd_setup, NULL,
		      NULL, NULL)) {
      g_free(argv);
      goto OUTTRUE;
    }
    else {
      gchar *delete_dropbox_cmdline;

      g_free(argv);

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

gchar *
nautilus_dropbox_common_get_platform() {
  /* this function is haxed, we don't support a plethora of
     platforms yet, so we don't need great platform detection */
  return g_strdup_printf("lnx-%s", sizeof(long) == 8 ? "x86_64" : "x86");
}
