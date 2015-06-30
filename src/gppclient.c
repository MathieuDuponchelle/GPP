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

  GPPClientTaskDoneHandler handler;
  gpointer user_data;
  const gchar *current_request;
  gint retries_left;
};

G_DEFINE_TYPE (GPPClient, gpp_client, G_TYPE_OBJECT);

static void
s_handle_backend (GPPClient *self)
{
  zmsg_t *msg = zmsg_recv (self->backend);
  const gchar *last_request;
  if (!msg) {
    return;
  }

  last_request = self->current_request;
  self->current_request = NULL;

  if (zmsg_size (msg) == 1) {
    zframe_t *frame = zmsg_first (msg);
    if (!memcmp (zframe_data (frame), PPP_KO, 1)) {
      g_debug ("Job failed");
      if (self->retries_left == 0) {
        g_info ("Failed, not retrying anymore");
        self->handler (self, NULL, FALSE, self->user_data);
      } else {
        if (self->retries_left != -1)
          self->retries_left--;
        g_debug ("Retrying, retries left : %d", self->retries_left);
        gpp_client_send_request (self,
            last_request, self->retries_left, self->handler, self->user_data);
      }
    } else {
      zframe_t *reply_frame = zmsg_last (msg);
      char *reply = zframe_strdup (reply_frame);
      self->handler (self, reply, TRUE, self->user_data);
      free (reply);
    }
  }


  zmsg_destroy (&msg);
}

static gboolean
socket_activity(GIOChannel *channel, GIOCondition condition, GPPClient *self)
{
  uint32_t status;
  size_t sizeof_status = sizeof(status);

  /* FIXME : error handling here, not sure what to do */

  if (zmq_getsockopt(self->backend, ZMQ_EVENTS, &status, &sizeof_status)) {
    perror("retrieving event status");
    return 0;
  }

  if ((status & ZMQ_POLLIN) != 0) {
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
gpp_client_send_request (GPPClient *self,
                         const gchar *request,
                         gint retries,
                         GPPClientTaskDoneHandler handler,
                         gpointer user_data)
{
  if (!handler)
    return FALSE;

  if (self->current_request)
    return FALSE;

  self->handler = handler;
  self->user_data = user_data;
  self->current_request = request;
  self->retries_left = retries;

  zstr_send (self->backend, request);
  return TRUE;
}
