#include <glib.h>
#include <glib-unix.h>
#include <gio/gio.h>
#include "czmq.h"

#define HEARTBEAT_LIVENESS  3
#define HEARTBEAT_INTERVAL  G_USEC_PER_SEC

#define PPP_READY       "\001"
#define PPP_HEARTBEAT   "\002"

typedef struct {
  /* Messaging */
  zctx_t *ctx;
  void *frontend;
  void *backend;
  GIOChannel *frontend_channel;
  guint frontend_source;

  /* Worker Management */
  GHashTable *workerz;
  GQueue *available_workerz;

  /* Main */
  GMainLoop *loop;
} ppqueue_t;

static gboolean callback_func(GIOChannel *source, GIOCondition condition, ppqueue_t *self);

/* Worker management */

typedef struct {
    zframe_t *identity;
    gchar *id_string;
    gint64 expiry;
} Worker;

static Worker *
worker_new (zframe_t *identity)
{
    Worker *self = g_slice_new (Worker);
    self->identity = identity;
    self->id_string = zframe_strhex (identity);
    return self;
}

static void
worker_destroy (Worker *self)
{
  zframe_destroy (&self->identity);
  free (self->id_string);
  g_slice_free (Worker, self);
}

static gboolean
maybe_purge_worker (gchar *id_string, Worker *worker, ppqueue_t *self)
{
  if (g_get_monotonic_time () > worker->expiry) {
    g_info ("purging worker with id %s\n", worker->id_string);
    g_queue_remove (self->available_workerz, worker);
    return TRUE;
  }
  return FALSE;
}

static void
purge_workers (ppqueue_t *self)
{
  g_hash_table_foreach_remove (self->workerz, (GHRFunc) maybe_purge_worker, self);
  if (g_queue_get_length (self->available_workerz) == 0 && self->frontend_source) {
    g_source_remove (self->frontend_source);
    self->frontend_source = 0;
  }
}

static void
add_available_worker (ppqueue_t *self, Worker *worker)
{
  g_debug ("worker %s is now available", worker->id_string);
  g_queue_push_tail (self->available_workerz, worker);
  if (g_queue_get_length (self->available_workerz) == 1)
    self->frontend_source = g_io_add_watch(self->frontend_channel,
        G_IO_IN, (GIOFunc) callback_func, self);
}

static Worker *
add_new_worker (ppqueue_t *self, zframe_t *identity)
{
  Worker *worker = worker_new (identity);
  g_hash_table_insert (self->workerz, worker->id_string, worker);
  g_info ("Created a new worker : %s", worker->id_string);
  add_available_worker (self, worker);
  return worker;
}

/* Messaging */

int s_handle_backend (ppqueue_t *self)
{
  zmsg_t *msg = zmsg_recv (self->backend);
  Worker *worker = NULL;
  if (!msg) {
    return -1;
  }

  zframe_t *identity = zmsg_unwrap (msg);

  //  Validate control message, or return reply to client

  worker = g_hash_table_lookup (self->workerz, zframe_strhex (identity));
  if (!worker)
    worker = add_new_worker (self, identity);

  if (zmsg_size (msg) == 1) {
    zframe_t *frame = zmsg_first (msg);

    if (memcmp (zframe_data (frame), PPP_READY, 1)
        &&  memcmp (zframe_data (frame), PPP_HEARTBEAT, 1)) {
      printf ("E: invalid message from worker");
      zmsg_dump (msg);
    }
    zmsg_destroy (&msg);
  }
  else {
    g_info ("worker %s has completed a task !", worker->id_string);
    zmsg_send (&msg, self->frontend);
    add_available_worker (self, worker);
  }

  worker->expiry = g_get_monotonic_time ()
                 + HEARTBEAT_INTERVAL * HEARTBEAT_LIVENESS;

  return 0;
}

int s_handle_frontend (ppqueue_t *self)
{
  Worker *worker;
  zframe_t *worker_id_dup;
  zmsg_t *msg = zmsg_recv (self->frontend);
  if (!msg) {
    return -1;
  }

  worker = g_queue_pop_head (self->available_workerz);
  if (g_queue_get_length (self->available_workerz) == 0) {
    g_source_remove (self->frontend_source);
    self->frontend_source = 0;
  }

  worker_id_dup = zframe_dup (worker->identity);
  zmsg_prepend (msg, &worker_id_dup);

  g_info ("sending task to worker %s", worker->id_string);
  zmsg_send (&msg, self->backend);
  return 0;
}

static gboolean callback_func(GIOChannel *source, GIOCondition condition, ppqueue_t *self)
{
  uint32_t status;
  size_t sizeof_status = sizeof(status);
  gboolean go_on;

  /* FIXME : error handling here, not sure what to do */
  do {
    go_on = FALSE;

    if (zmq_getsockopt(self->backend, ZMQ_EVENTS, &status, &sizeof_status)) {
      perror("retrieving event status");
      return 0;
    }

    if ((status & ZMQ_POLLIN) != 0) {
      s_handle_backend (self);
      go_on = TRUE;
    }

    if (!self->frontend_source)
      continue;

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

/* Heartbeating */

static void
send_heartbeat (gchar *id_string, Worker *worker, ppqueue_t *self)
{
  zframe_send (&worker->identity, self->backend,
      ZFRAME_REUSE + ZFRAME_MORE);
  zframe_t *frame = zframe_new (PPP_HEARTBEAT, 1);
  zframe_send (&frame, self->backend, 0);
}

static gboolean
do_heartbeat (ppqueue_t *self)
{
  g_hash_table_foreach (self->workerz, (GHFunc) send_heartbeat, self);

  g_debug ("doing heartbeat\n");
  purge_workers (self);
  return TRUE;
}

/* Initialization */

static GIOChannel *
g_io_channel_from_zmq_socket (void *socket)
{
  int fd;
  size_t sizeof_fd = sizeof(fd);
  if(zmq_getsockopt(socket, ZMQ_FD, &fd, &sizeof_fd))
    perror("retrieving zmq fd");
  return g_io_channel_unix_new(fd);
}

static void
create_channels (ppqueue_t *self)
{
  self->ctx = zctx_new ();
  self->frontend = zsocket_new (self->ctx, ZMQ_ROUTER);
  self->backend = zsocket_new (self->ctx, ZMQ_ROUTER);
  zsocket_bind (self->frontend, "tcp://*:5555");
  zsocket_bind (self->backend,  "tcp://*:5556");

  self->frontend_channel = g_io_channel_from_zmq_socket (self->frontend);
  g_io_add_watch (g_io_channel_from_zmq_socket (self->backend),
      G_IO_IN, (GIOFunc) callback_func, self);
}

/* Main */

static gboolean
interrupted_cb (ppqueue_t *self)
{
  g_main_loop_quit (self->loop);
  return FALSE;
}

int main (void)
{
  ppqueue_t *self = g_malloc0 (sizeof (ppqueue_t));

  create_channels (self);
  self->loop = g_main_loop_new (NULL, FALSE);
  self->workerz = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) worker_destroy);
  self->available_workerz = g_queue_new();

  g_unix_signal_add_full (G_PRIORITY_DEFAULT, SIGINT, (GSourceFunc) interrupted_cb, self, NULL);
  g_timeout_add (HEARTBEAT_INTERVAL / 1000, (GSourceFunc) do_heartbeat, self);
  g_main_loop_run (self->loop);

  g_hash_table_unref (self->workerz);
  zctx_destroy (&self->ctx);
  return 0;
}
