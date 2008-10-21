/*
 * Copyright 2008 Evenflow, Inc.
 *
 * g-util.c
 * glib/gdk/gtk dependent utility functions.
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gprintf.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "g-util.h"

void
g_util_destroy_string(gpointer data) {
  g_string_free((GString *) data, TRUE);
  return;
}

gboolean g_util_parse_url(const gchar *url,
			  gchar **scheme,
			  gchar **hostname,
			  gint *port,
			  gchar **path) {
  char *first_ptr, *colon_ptr, *slash_ptr;

  /* find the :// */
  first_ptr = strstr(url, "://");
  if (first_ptr == NULL) {
    return FALSE;
  }

  *scheme = g_strndup(url, (first_ptr - url));

  /* now try to parse out the hostname and port */
  first_ptr += 3;
  colon_ptr = strstr(first_ptr, ":");
  slash_ptr = strstr(first_ptr, "/");

  /* we are over explicit about our cases here,
     don't worry, gcc is smart about optimizing these comparisons
     we want the code to be easy to read */
  if (colon_ptr == NULL && slash_ptr == NULL) {
    /* TODO: verify hostname */
    *hostname = g_strdup(first_ptr);
    *port = -1;
    *path = NULL;
  }
  else if (colon_ptr == NULL && slash_ptr != NULL) {
    *hostname = g_strndup(first_ptr, slash_ptr - first_ptr);
    *port = -1;
    *path = g_strdup(slash_ptr);
  }
  /* technically not a valid URL, but we'll let it slide */
  else if (colon_ptr != NULL && slash_ptr == NULL) {
    long tmp_port;
    char *endptr;

    errno = 0;
    tmp_port = strtol(colon_ptr + 1, &endptr, 10);

    /* we basically want the rest of the string to be a valid port number */
    if (tmp_port < 0 ||
	tmp_port > 65535 ||
	*endptr != '\0' ||
	errno != 0) {
      g_free(*scheme);
      return FALSE;
    }

    *hostname = g_strndup(first_ptr, colon_ptr - first_ptr);
    *port = (int) tmp_port;
    *path = NULL;
  }
  /* colon comes before first slash, possibility for a port num, otherwise
     could be dealing with an invalid url */
  else if (colon_ptr < slash_ptr) {
    long tmp_port;
    char *endptr;

    errno = 0;
    tmp_port = strtol(colon_ptr + 1, &endptr, 10);

    /* error condition */
    if (endptr != slash_ptr || /* didn't stop at the slash */
	tmp_port < 0 ||  /* negative port number ? */
	errno != 0 || /* port was out of range */
	tmp_port > 65535) { /* port was out of range */
      g_free(*scheme);
      return FALSE;
    }

    *hostname = g_strndup(first_ptr, colon_ptr - first_ptr);
    *port = (gint) tmp_port;
    *path = g_strdup(slash_ptr);
  }
  /* this last case has to be colon_ptr > slash_ptr,
     seriously can't be a port at that point */
  else {
    *hostname = g_strndup(first_ptr, slash_ptr - first_ptr);
    *port = -1;
    *path = g_strdup(slash_ptr);
  }

  /* verify scheme */
  {
    char *scheme_ptr;

    for (scheme_ptr = *scheme; *scheme_ptr != '\0'; scheme_ptr++) {
      if (g_ascii_isalnum(*scheme_ptr) == FALSE &&
	  /* ascii "+" */
	  *scheme_ptr != 0x2B &&
	  /* ascii "-" */
	  *scheme_ptr != 0x2D &&
	  /* ascii "." */
	  *scheme_ptr != 0x2E) {
	g_free(*scheme);
	g_free(*hostname);
	g_free(*path);
	return FALSE;
      }
    }
  }

  /* verify hostname */    
  {
    char *hostname_ptr;

    for (hostname_ptr = *hostname; *hostname_ptr != '\0';
	 hostname_ptr++) {
      if (g_ascii_isalnum(*hostname_ptr) == FALSE &&
	  /* ascii "-" */
	  *hostname_ptr != 0x2D &&
	  /* ascii "." */
	  *hostname_ptr != 0x2E) {
	g_free(*scheme);
	g_free(*hostname);
	g_free(*path);
	return FALSE;
      }
    }

    /* TODO: we don't check the length of each "label" or subdomain 
       the parts between the periods */
    if (hostname_ptr - *hostname > 255) {
	g_free(*scheme);
	g_free(*hostname);
	g_free(*path);
	return FALSE;
    }
  }

  return TRUE;
}
