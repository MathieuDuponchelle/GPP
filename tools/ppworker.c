#include "gppworker.h"

#include "czmq.h"
#define HEARTBEAT_LIVENESS  3
#define HEARTBEAT_INTERVAL  G_USEC_PER_SEC
#define INTERVAL_INIT       1000
#define INTERVAL_MAX       32000

#define PPP_READY       "\001"
#define PPP_HEARTBEAT   "\002"

struct _GPPWorker
{
  GObject parent;
  zctx_t *ctx;

  void *frontend;
  GIOChannel *frontend_channel;
  guint frontend_source;

  guint liveness;
  guint interval;
};

G_DEFINE_TYPE (GPPWorker, gpp_worker, G_TYPE_OBJECT);

/* Messaging */

  static void
s_handle_frontend (GPPWorker *self)
{
  zmsg_t *msg = zmsg_recv (self->frontend);
  if (!msg)
    return;

  if (zmsg_size (msg) == 3) {
    printf ("I: normal reply\n");
    zmsg_send (&msg, self->frontend);
    self->liveness = HEARTBEAT_LIVENESS;
    sleep (1);              //  Do some heavy work
  } else {
    if (zmsg_size (msg) == 1) {
      zframe_t *frame = zmsg_first (msg);
      if (memcmp (zframe_data (frame), PPP_HEARTBEAT, 1) == 0) {
        self->liveness = HEARTBEAT_LIVENESS;
        printf ("got heartbeat from queue !\n");
      } else {
        printf ("E: invalid message\n");
        zmsg_dump (msg);
      }
      zmsg_destroy (&msg);
    }
    else {
      printf ("E: invalid message\n");
      zmsg_dump (msg);
    }
  }
  self->interval = INTERVAL_INIT;
}

static gboolean callback_func(GIOChannel *channel, GIOCondition condition, GPPWorker *self)
{
  uint32_t status;
  size_t sizeof_status = sizeof(status);
  gboolean go_on;

  /* FIXME : error handling here, not sure what to do */
  do {
    go_on = FALSE;

    if (zmq_getsockopt(self->frontend, ZMQ_EVENTS, &status, &sizeof_status)) {
      perror("retrieving event status");
      return 0;
    }

    if ((status & ZMQ_POLLIN) != 0) {
      go_on = TRUE;
      s_handle_frontend (self);
    }
  } while (go_on);

  return 1;
}

  static GIOChannel *
g_io_channel_from_zmq_socket (void *socket)
{
  int fd;
  size_t sizeof_fd = sizeof(fd);
  if (zmq_getsockopt(socket, ZMQ_FD, &fd, &sizeof_fd))
    perror("retrieving zmq fd");

  return g_io_channel_unix_new(fd);
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

  GPPWorker *
gpp_worker_new (void)
{
  return g_object_new (GPP_TYPE_WORKER, NULL);
}

static gboolean do_heartbeat (GPPWorker *self);

static gboolean
do_start (GPPWorker *self)
{
  self->frontend = zsocket_new (self->ctx, ZMQ_DEALER);
  zsocket_connect (self->frontend, "tcp://localhost:5556");
  self->frontend_channel = g_io_channel_from_zmq_socket (self->frontend);
  self->liveness = HEARTBEAT_LIVENESS;
  self->frontend_source = g_io_add_watch (self->frontend_channel,
      G_IO_IN, (GIOFunc) callback_func, self);

  g_timeout_add (HEARTBEAT_INTERVAL / 1000, (GSourceFunc) do_heartbeat, self);

  zframe_t *frame = zframe_new (PPP_READY, 1);
  zframe_send (&frame, self->frontend, 0);

  /* We need to do that for some reason ... */
  callback_func (self->frontend_channel, G_IO_IN, self);

  return FALSE;
}

static gboolean
do_heartbeat (GPPWorker *self)
{
  if (--self->liveness == 0) {
    printf ("W: heartbeat failure, can't reach queue\n");
    printf ("W: reconnecting in %zd msec...\n", self->interval);
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
  callback_func (self->frontend_channel, G_IO_IN, self);
  return TRUE;
}

gboolean
gpp_worker_start (GPPWorker *self)
{
  if (self->frontend_source)
    return FALSE;

  do_start (self);

  return TRUE;
}

int main (void)
{
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  GPPWorker *self = gpp_worker_new();

  gpp_worker_start (self);

  g_main_loop_run (loop);
  return 0;
}
