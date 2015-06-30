#include "gpputils.h"
#include "gppworker.h"

#include "czmq.h"

#define INTERVAL_INIT       1000
#define INTERVAL_MAX       32000

/* Structure definitions */

/**
 * SECTION: gppworker
 *
 * #GPPWorker receives requests from a #GPPQueue, transmits them
 * as a simple string to the user, and notifies the queue when the
 * user marks the task as done.
 *
 * {{ ppworker.markdown }}
 */

#define GET_PRIV(self) (gpp_worker_get_instance_private (GPP_WORKER (self)))

typedef struct _GPPWorkerPrivate
{
  zctx_t *ctx;

  void *frontend;
  GIOChannel *frontend_channel;
  guint frontend_source;

  guint liveness;
  guint interval;
  zmsg_t *current_task;
} GPPWorkerPrivate;

G_DEFINE_TYPE_WITH_CODE (GPPWorker, gpp_worker, G_TYPE_OBJECT,
    G_ADD_PRIVATE (GPPWorker));

/* Messaging */

static void
handle_frontend (GPPWorker *self)
{
  GPPWorkerPrivate *priv = GET_PRIV (self);
  GPPWorkerClass *klass = GPP_WORKER_GET_CLASS (self);
  zmsg_t *msg = zmsg_recv (priv->frontend);
  if (!msg)
    return;

  if (zmsg_size (msg) == 3) {
    char *request = zframe_strdup (zmsg_last (msg));
    g_info ("I: normal reply\n");
    priv->liveness = HEARTBEAT_LIVENESS;
    priv->current_task = msg;
    if (!klass->handle_request (self, request))
      gpp_worker_set_task_done (self, NULL, FALSE);
    free (request);
  } else {
    if (zmsg_size (msg) == 1) {
      zframe_t *frame = zmsg_first (msg);
      if (memcmp (zframe_data (frame), PPP_HEARTBEAT, 1) == 0) {
        priv->liveness = HEARTBEAT_LIVENESS;
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
  priv->interval = INTERVAL_INIT;
}

static gboolean
check_socket_activity(GIOChannel *channel, GIOCondition condition, GPPWorker *self)
{
  GPPWorkerPrivate *priv = GET_PRIV (self);
  uint32_t status;
  size_t sizeof_status = sizeof(status);
  gboolean go_on;

  /* FIXME : error handling here, not sure what to do */
  go_on = FALSE;

  if (zmq_getsockopt(priv->frontend, ZMQ_EVENTS, &status, &sizeof_status)) {
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
  GPPWorkerPrivate *priv = GET_PRIV (self);
  priv->frontend = zsocket_new (priv->ctx, ZMQ_DEALER);
  zsocket_connect (priv->frontend, "tcp://localhost:5556");
  priv->frontend_channel = g_io_channel_from_zmq_socket (priv->frontend);
  priv->liveness = HEARTBEAT_LIVENESS;
  priv->frontend_source = g_io_add_watch (priv->frontend_channel,
      G_IO_IN, (GIOFunc) check_socket_activity, self);

  g_timeout_add (HEARTBEAT_INTERVAL / 1000, (GSourceFunc) do_heartbeat, self);

  zframe_t *frame = zframe_new (PPP_READY, 1);
  zframe_send (&frame, priv->frontend, 0);

  /* We need to do that for some reason ... */
  check_socket_activity (priv->frontend_channel, G_IO_IN, self);

  return FALSE;
}

static gboolean
do_heartbeat (GPPWorker *self)
{
  GPPWorkerPrivate *priv = GET_PRIV (self);
  if (--priv->liveness == 0) {
    g_warning ("W: heartbeat failure, can't reach queue\n");
    g_warning ("W: reconnecting in %zd msec...\n", priv->interval);
    g_source_remove (priv->frontend_source);
    priv->frontend_source = 0;
    g_io_channel_unref (priv->frontend_channel);

    if (priv->interval < INTERVAL_MAX)
      priv->interval *= 2;

    zsocket_destroy (priv->ctx, priv->frontend);
    g_timeout_add (priv->interval, (GSourceFunc) do_start, self);
    return FALSE;
  }

  zframe_t *frame = zframe_new (PPP_HEARTBEAT, 1);
  zframe_send (&frame, priv->frontend, 0);
  /* We need to do that for some reason ... */
  check_socket_activity (priv->frontend_channel, G_IO_IN, self);
  return TRUE;
}

/* GObject */

static void
dispose (GObject *object)
{
  GPPWorker *self = GPP_WORKER (object);
  GPPWorkerPrivate *priv = GET_PRIV (self);

  zctx_destroy (&priv->ctx);
}

static void
gpp_worker_class_init (GPPWorkerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  klass->handle_request = NULL;
  gobject_class->dispose = dispose;
}

static void
gpp_worker_init (GPPWorker *self)
{
  GPPWorkerPrivate *priv = GET_PRIV (self);
  priv->interval = INTERVAL_INIT;
  priv->ctx = zctx_new ();
}

/* API */

/**
 * gpp_worker_set_task_done:
 * @self: A #GPPWorker.
 * @reply: (allow-none): A string that will be passed to the client.
 * @success: Whether the task was successfully handled.
 *
 * Call this function when your worker has finished handling a task.
 *
 * Returns: %TRUE if the task was marked as done, %FALSE otherwise.
 */
gboolean
gpp_worker_set_task_done (GPPWorker *self, const gchar *reply, gboolean success)
{
  GPPWorkerPrivate *priv = GET_PRIV (self);
  zframe_t *request_frame = zmsg_last (priv->current_task);

  if (!priv->current_task)
    return FALSE;

  if (!success) {
    zframe_reset (request_frame, PPP_KO, 1);
  } else {
    zframe_reset (request_frame, reply, strlen (reply) + 1);
  }

  zmsg_send (&priv->current_task, priv->frontend);
  priv->current_task = NULL;
  check_socket_activity (priv->frontend_channel, G_IO_IN, self);
  return TRUE;
}

/**
 * gpp_worker_start:
 * @self: A #GPPWorker that will start handling requests.
 *
 * This will make @self start handling requests.
 *
 * Returns: %TRUE if @self could be started, %FALSE if it was already.
 */
gboolean
gpp_worker_start (GPPWorker *self)
{
  GPPWorkerPrivate *priv = GET_PRIV (self);
  GPPWorkerClass *klass = GPP_WORKER_GET_CLASS (self);
  if (priv->frontend_source)
    return FALSE;

  if (!klass->handle_request)
    return FALSE;

  do_start (self);

  return TRUE;
}

/**
 * gpp_worker_new:
 *
 * Create a new #GPPWorker, which doesn't yet listen to request from
 * the #GPPQueue.
 * Start it with gpp_worker_start()
 *
 * Returns: the newly-created #GPPWorker.
 */
GPPWorker *
gpp_worker_new (void)
{
  return g_object_new (GPP_TYPE_WORKER, NULL);
}
