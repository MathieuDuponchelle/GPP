#ifndef _GPP_CLIENT
#define _GPP_CLIENT

#include <glib-object.h>

G_BEGIN_DECLS

#define GPP_TYPE_CLIENT (gpp_client_get_type ())

G_DECLARE_FINAL_TYPE(GPPClient, gpp_client, GPP, CLIENT, GObject)

typedef void (*GPPClientTaskDoneHandler)(GPPClient *, const gchar *reply, gboolean success, gpointer user_data);

GPPClient * gpp_client_new (void);
gboolean gpp_client_send_request (GPPClient *self,
                                  const gchar *request,
                                  gint retries);

#endif
