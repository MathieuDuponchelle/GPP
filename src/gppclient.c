#include <gio/gio.h>
#include <czmq.h>

#include "gpputils.h"
#include "gppclient.h"

#define REQUEST_RETRIES     3
#define SERVER_ENDPOINT     "tcp://localhost:5555"

struct _GPPClient
{
  GObject parent;
  zctx_t *ctx;

  void *backend;
  guint backend_source;
};

G_DEFINE_TYPE (GPPClient, gpp_client, G_TYPE_OBJECT);

static void
s_handle_backend (GPPClient *self)
{
  char *reply = zstr_recv (self->backend);
  if (!reply)
    return;

  g_info ("Server replied %s\n", reply);

  free (reply);
}

static gboolean socket_activity(GIOChannel *channel, GIOCondition condition, GPPClient *self)
{
  uint32_t status;
  size_t sizeof_status = sizeof(status);
  gboolean go_on;

  /* FIXME : error handling here, not sure what to do */
  go_on = FALSE;

  if (zmq_getsockopt(self->backend, ZMQ_EVENTS, &status, &sizeof_status)) {
    perror("retrieving event status");
    return 0;
  }

  if ((status & ZMQ_POLLIN) != 0) {
    go_on = TRUE;
    s_handle_backend (self);
  }

  return 1;
}

static void
dispose (GObject *object)
{
  GPPClient *self = GPP_CLIENT (object);

  zctx_destroy (&self->ctx);
}

static void
gpp_client_class_init (GPPClientClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = dispose;
}

static void
gpp_client_init (GPPClient *self)
{
  self->ctx = zctx_new ();
  self->backend = zsocket_new (self->ctx, ZMQ_REQ);
  zsocket_connect (self->backend, SERVER_ENDPOINT);
  self->backend_source = g_io_add_watch (g_io_channel_from_zmq_socket (self->backend),
      G_IO_IN, (GIOFunc) socket_activity, self);
}

GPPClient *
gpp_client_new (void)
{
  return g_object_new (GPP_TYPE_CLIENT, NULL);
}

gboolean
gpp_client_send_request (GPPClient *self)
{
  char request [10];
  sprintf (request, "%d", 2);
  zstr_send (self->backend, request);
  return TRUE;
}
