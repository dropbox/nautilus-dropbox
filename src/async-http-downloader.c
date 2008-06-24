#include <unistd.h>

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "async-http-downloader.h"
#include "async-io-coroutine.h"

  /* this is a sneaky function, we don't do any of the http connection
     ourselves, just call wget and parse it's output

     why? wget probably has better http code than we should ever bother
     writing :)
  */

typedef struct {
  HttpResponseHandler cb;
  gpointer ud;
  GIOChannel *stdout_chan;
  struct {
    int codepos;
    int response_code;
    GList *response_headers;
  } readctx;
} WgetAsyncHttpRequest;

static void setClocale(gpointer ud) {
  /* we do this so wget's output can be interpreted as utf8 encoding */
  g_setenv("LC_ALL", "C", TRUE);
}

static void free_wahp(WgetAsyncHttpRequest *wahp) {
  /* do all the wahp cleanup here, who knows if
     a readline could fail */
  g_io_channel_unref(wahp->stdout_chan);
  
  /* this may look incorrect but there is no harm in calling
     a function that takes one argument as a function that takes
     two arguments in C, take it from the glib hackers ;) */
  g_list_foreach(wahp->readctx.response_headers, (GFunc) g_free, NULL);
  g_list_free(wahp->readctx.response_headers);

  g_free(wahp);
}

static gboolean
handle_wget_stderr(GIOChannel *stderr_chan,
                   GIOCondition cond,
                   WgetAsyncHttpRequest *wahp) {

  if (cond == G_IO_ERR || cond == G_IO_HUP) {
    return FALSE;
  }

  CRBEGIN(wahp->readctx.codepos);

  /* read line until we get the first line that starts with "  " */
  while (1) {
    gchar *line;

    CRREADLINE(wahp->readctx.codepos, stderr_chan, line);

    /* first response line is the HTTP response code */
    /* we can character manipulation like this because
       wget should be pumping only 7-bit ascii to us
       (remember we set the locale?) */
    if (strncmp("  ", line, 2) == 0) {
      line[14] = '\0';
      /* we could also probably extract out the http version and
         response message if need be */
      wahp->readctx.response_code = atoi(&line[11]);
      g_free(line);
      break;
    }
    else {
      g_free(line);
    }
  }

  /* the rest of the lines are headers, until the first line that
     doesn't start with "  " */
  while (1) {
    gchar *line;

    CRREADLINE(wahp->readctx.codepos, stderr_chan, line);
    
    if (strncmp("  ", line, 2) == 0) {
      wahp->readctx.response_headers = 
        g_list_append(wahp->readctx.response_headers, line);
    }
    else {
      g_free(line);
      break;
    }
  }

  /* great got all the response headers and the response code
     we are basically just, just call our callback */
  wahp->cb(wahp->readctx.response_code,
           wahp->readctx.response_headers,
           wahp->stdout_chan, wahp->ud);

  CREND;
}

gboolean
make_async_http_get_request(const char *host, const char *path,
                            GList *request_headers,
                            HttpResponseHandler cb, gpointer ud) {
  gint stdout, stderr;
  gchar **argv;

  g_assert(host != NULL);
  g_assert(path != NULL);

  /* first build argv */
  {
    int i = 0;
    int argv_length;

    argv_length = 6 + g_list_length(request_headers);

    argv = g_new(gchar *, argv_length);
    argv[i++] = g_strdup("wget");
    argv[i++] = g_strdup("-O-");
    argv[i++] = g_strdup("--server-response");
    argv[i++] = g_strdup("--max-redirect=0");

    {
      GList *li;
      for (li = request_headers; li != NULL; li = g_list_next(li)) {
        argv[i++] = g_strdup_printf("--header=%s", (gchar *) li->data);
      }
    }

    argv[i++] = g_strdup_printf("http://%s%s", host, path);
    argv[i++] = NULL;

    g_assert(i == argv_length);
  }
  
  /* spawn process and create io channels */
  if (!g_spawn_async_with_pipes(NULL, argv, NULL,
                                G_SPAWN_SEARCH_PATH, setClocale, NULL, NULL,
                                NULL, &stdout, &stderr, NULL)) {
    return FALSE;
  }

  /* the call succeeded, let's install our strerr handlers */
  {
    WgetAsyncHttpRequest *wahp;
    GIOChannel *stderr_chan;

    stderr_chan = g_io_channel_unix_new(stderr);
    g_io_channel_set_close_on_unref(stderr_chan, TRUE);
    g_io_channel_set_line_term(stderr_chan, "\n", -1);

    /* set stderr to non-blocking ;) */
    {
      GIOFlags flags;
      GError *gerr = NULL;
      
      flags = g_io_channel_get_flags(stderr_chan);
      g_io_channel_set_flags(stderr_chan, flags | G_IO_FLAG_NONBLOCK,
                             &gerr);
      if (gerr != NULL) {
        close(stdout);
        g_io_channel_unref(stderr_chan);
        g_error_free(gerr);
        return FALSE;
      }
    }
    
    wahp = g_new(WgetAsyncHttpRequest, 1);
    wahp->cb = cb;
    wahp->ud = ud;
    wahp->stdout_chan = g_io_channel_unix_new(stdout);
    g_io_channel_set_close_on_unref(wahp->stdout_chan, TRUE);

    wahp->readctx.codepos = 0;
    wahp->readctx.response_code = -1;
    wahp->readctx.response_headers = NULL;
    g_io_add_watch_full(stderr_chan, G_PRIORITY_DEFAULT,
                        G_IO_IN | G_IO_ERR | G_IO_HUP,
                        (GIOFunc) handle_wget_stderr, wahp,
                        (GDestroyNotify) free_wahp);

    g_io_channel_unref(stderr_chan);
  }

  return TRUE;
}
