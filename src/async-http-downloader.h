#ifndef ASYNC_HTTP_DOWNLOADER_H
#define ASYNC_HTTP_DOWNLOADER_H

#include <glib.h>

G_BEGIN_DECLS

typedef void (*HttpResponseHandler)(gint response_code,
                                    GList *headers,
                                    GIOChannel *data_channel,
                                    gpointer ud);

gboolean
make_async_http_get_request(const char *host, const char *path,
                            GList *request_headers,
                            HttpResponseHandler cb, gpointer ud);

G_END_DECLS

#endif
