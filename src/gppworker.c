#include "gpputils.h"
#include "gppworker.h"

#include "czmq.h"

#define INTERVAL_INIT       1000
#define INTERVAL_MAX       32000

/* Structure definitions */

struct _GPPWorker
{
  GObject parent;
  zctx_t *ctx;

  void *frontend;
  GIOChannel *frontend_channel;
  guint frontend_source;

  guint liveness;
  guint interval;
  zmsg_t *current_task;
  GPPWorkerTaskHandler handler;
  gpointer user_data;
};

G_DEFINE_TYPE (GPPWorker, gpp_worker, G_TYPE_OBJECT);

/* Messaging */

static void
handle_frontend (GPPWorker *self)
{
  zmsg_t *msg = zmsg_recv (self->frontend);
  if (!msg)
    return;

  if (zmsg_size (msg) == 3) {
    char *request = zframe_strdup (zmsg_last (msg));
    g_info ("I: normal reply\n");
    self->liveness = HEARTBEAT_LIVENESS;
    self->current_task = msg;
    if (!self->handler (self, request, self->user_data))
      gpp_worker_set_task_done (self, NULL, FALSE);
    free (request);
  } else {
    if (zmsg_size (msg) == 1) {
      zframe_t *frame = zmsg_first (msg);
      if (memcmp (zframe_data (frame), PPP_HEARTBEAT, 1) == 0) {
        self->liveness = HEARTBEAT_LIVENESS;
        g_debug ("got heartbeat from queue !\n");
      } else {
        g_warning ("E: invalid message\n");
        zmsg_dump (msg);
      }
      zmsg_destroy (&msg);
    }
    else {
      g_warning ("E: invalid message\n");
      zmsg_dump (msg);
    }
  }
  self->interval = INTERVAL_INIT;
}

static gboolean
check_socket_activity(GIOChannel *channel, GIOCondition condition, GPPWorker *self)
{
  uint32_t status;
  size_t sizeof_status = sizeof(status);
  gboolean go_on;

  /* FIXME : error handling here, not sure what to do */
  go_on = FALSE;

  if (zmq_getsockopt(self->frontend, ZMQ_EVENTS, &status, &sizeof_status)) {
    perror("retrieving event status");
    return 0;
  }

  if ((status & ZMQ_POLLIN) != 0) {
    go_on = TRUE;
    handle_frontend (self);
  }

  return 1;
}

/* Heartbeating / Reconnecting */

static gboolean do_heartbeat (GPPWorker *self);

static gboolean
do_start (GPPWorker *self)
{
  self->frontend = zsocket_new (self->ctx, ZMQ_DEALER);
  zsocket_connect (self->frontend, "tcp://localhost:5556");
  self->frontend_channel = g_io_channel_from_zmq_socket (self->frontend);
  self->liveness = HEARTBEAT_LIVENESS;
  self->frontend_source = g_io_add_watch (self->frontend_channel,
      G_IO_IN, (GIOFunc) check_socket_activity, self);

  g_timeout_add (HEARTBEAT_INTERVAL / 1000, (GSourceFunc) do_heartbeat, self);

  zframe_t *frame = zframe_new (PPP_READY, 1);
  zframe_send (&frame, self->frontend, 0);

  /* We need to do that for some reason ... */
  check_socket_activity (self->frontend_channel, G_IO_IN, self);

  return FALSE;
}

static gboolean
do_heartbeat (GPPWorker *self)
{
  if (--self->liveness == 0) {
    g_warning ("W: heartbeat failure, can't reach queue\n");
    g_warning ("W: reconnecting in %zd msec...\n", self->interval);
    g_source_remove (self->frontend_source);
    self->frontend_source = 0;
    g_io_channel_unref (self->frontend_channel);

    if (self->interval < INTERVAL_MAX)
      self->interval *= 2;

    zsocket_destroy (self->ctx, self->frontend);
    g_timeout_add (self->interval, (GSourceFunc) do_start, self);
    return FALSE;
  }

  zframe_t *frame = zframe_new (PPP_HEARTBEAT, 1);
  zframe_send (&frame, self->frontend, 0);
  /* We need to do that for some reason ... */
  check_socket_activity (self->frontend_channel, G_IO_IN, self);
  return TRUE;
}

/* GObject */

static void
dispose (GObject *object)
{
  GPPWorker *self = GPP_WORKER (object);

  zctx_destroy (&self->ctx);
}

static void
gpp_worker_class_init (GPPWorkerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = dispose;
}

static void
gpp_worker_init (GPPWorker *self)
{
  self->interval = INTERVAL_INIT;
  self->ctx = zctx_new ();
}

/* API */

gboolean
gpp_worker_set_task_done (GPPWorker *self, const gchar *reply, gboolean success)
{
  zframe_t *request_frame = zmsg_last (self->current_task);

  if (!self->current_task)
    return FALSE;

  if (!success) {
    zframe_reset (request_frame, PPP_KO, 1);
  } else {
    zframe_reset (request_frame, reply, strlen (reply) + 1);
  }

  zmsg_send (&self->current_task, self->frontend);
  self->current_task = NULL;
  check_socket_activity (self->frontend_channel, G_IO_IN, self);
  return TRUE;
}

gboolean
gpp_worker_start (GPPWorker *self, GPPWorkerTaskHandler handler, gpointer user_data)
{
  if (self->frontend_source)
    return FALSE;

  if (!handler)
    return FALSE;

  self->handler = handler;
  self->user_data = user_data;
  do_start (self);

  return TRUE;
}

GPPWorker *
gpp_worker_new (void)
{
  return g_object_new (GPP_TYPE_WORKER, NULL);
}
