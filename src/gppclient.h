#ifndef _GPP_CLIENT
#define _GPP_CLIENT

#include <glib-object.h>

G_BEGIN_DECLS

#define GPP_TYPE_CLIENT (gpp_client_get_type ())

G_DECLARE_FINAL_TYPE(GPPClient, gpp_client, GPP, CLIENT, GObject)

typedef gboolean (*GPPClientTaskHandler)(GPPClient *, gpointer user_callback);

GPPClient * gpp_client_new (void);
gboolean gpp_client_send_request (GPPClient *self);

#endif
