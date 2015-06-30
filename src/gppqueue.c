/* GObject Paranoid Pirate
 * Copyright (C) 2015 Mathieu Duponchelle <mathieu.duponchelle@opencreed.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <glib.h>
#include <gio/gio.h>
#include <czmq.h>

#include "gpputils.h"
#include "gppqueue.h"

/**
 * SECTION: Introduction
 *
 * {{ introduction.markdown }}
 */

/**
 * SECTION: gppqueue
 *
 * #GPPQueue routes requests from #GPPClient (s) to #GPPWorker (s).
 *
 * {{ ppqueue.markdown }}
 *
 * It will detect if a worker stopped answering heartbeats, and
 * inform the client it was working for if there was one.
 *
 * It will pick workers on a least-recently-used basis.
 */

struct _GPPQueue
{
  GObject parent;

  /* Messaging */
  zctx_t *ctx;
  void *frontend;
  void *backend;
  GIOChannel *frontend_channel;
  guint frontend_source;
  GIOChannel *backend_channel;
  guint backend_source;

  /* Worker Management */
  GHashTable *workerz;
  GQueue *available_workerz;
};

G_DEFINE_TYPE (GPPQueue, gpp_queue, G_TYPE_OBJECT)

static gboolean check_socket_activity(GIOChannel *source, GIOCondition condition, GPPQueue *self);

/* Worker management */

typedef struct {
    zframe_t *identity;
    gchar *id_string;
    gint64 expiry;
    zframe_t *current_client;
} Worker;

static Worker *
worker_new (zframe_t *identity)
{
    Worker *self = g_slice_new (Worker);
    self->identity = identity;
    self->id_string = zframe_strhex (identity);
    self->current_client = NULL;
    return self;
}

static void
worker_destroy (Worker *self)
{
  zframe_destroy (&self->identity);
  free (self->id_string);
  if (self->current_client)
    zframe_destroy (&self->current_client);
  g_slice_free (Worker, self);
}

static gboolean
maybe_purge_worker (gchar *id_string, Worker *worker, GPPQueue *self)
{
  if (g_get_monotonic_time () > worker->expiry) {
    g_info ("purging worker with id %s", worker->id_string);
    if (worker->current_client) {
      zframe_t *client_id_dup = zframe_dup (worker->current_client);
      zframe_t *ko_frame = zframe_new (PPP_KO, 1);
      zframe_t *empty_frame = zframe_new_empty ();
      zmsg_t *msg = zmsg_new();
      zmsg_prepend (msg, &ko_frame);
      zmsg_prepend (msg, &empty_frame);
      zmsg_prepend (msg, &client_id_dup);
      zmsg_send (&msg, self->frontend);

      g_info ("Worker had a client, sent KO message");
    }

    g_queue_remove (self->available_workerz, worker);
    return TRUE;
  }
  return FALSE;
}

static void
purge_workers (GPPQueue *self)
{
  g_hash_table_foreach_remove (self->workerz, (GHRFunc) maybe_purge_worker, self);
  if (g_queue_get_length (self->available_workerz) == 0 && self->frontend_source) {
    g_source_remove (self->frontend_source);
    self->frontend_source = 0;
  }
}

static void
add_available_worker (GPPQueue *self, Worker *worker)
{
  g_debug ("worker %s is now available", worker->id_string);
  g_queue_push_tail (self->available_workerz, worker);
  if (g_queue_get_length (self->available_workerz) == 1)
    self->frontend_source = g_io_add_watch(self->frontend_channel,
        G_IO_IN, (GIOFunc) check_socket_activity, self);
}

static Worker *
add_new_worker (GPPQueue *self, zframe_t *identity)
{
  Worker *worker = worker_new (identity);
  g_hash_table_insert (self->workerz, worker->id_string, worker);
  g_info ("Created a new worker : %s", worker->id_string);
  add_available_worker (self, worker);
  return worker;
}

/* Messaging */

int handle_backend (GPPQueue *self)
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
      zmsg_dump (msg);
    }
    zmsg_destroy (&msg);
  }
  else {
    g_info ("worker %s has completed a task !", worker->id_string);
    zmsg_send (&msg, self->frontend);
    zframe_destroy (&worker->current_client);
    worker->current_client = NULL;
    add_available_worker (self, worker);
  }

  worker->expiry = g_get_monotonic_time ()
                 + HEARTBEAT_INTERVAL * HEARTBEAT_LIVENESS;

  return 0;
}

static void
handle_frontend (GPPQueue *self)
{
  Worker *worker;
  zframe_t *worker_id_dup;
  zmsg_t *msg = zmsg_recv (self->frontend);

  if (!msg)
    return;

  worker = g_queue_pop_head (self->available_workerz);
  if (g_queue_get_length (self->available_workerz) == 0) {
    g_source_remove (self->frontend_source);
    self->frontend_source = 0;
  }

  worker->current_client = zframe_dup (zmsg_first(msg));
  worker_id_dup = zframe_dup (worker->identity);
  zmsg_prepend (msg, &worker_id_dup);

  g_info ("sending task to worker %s", worker->id_string);
  zmsg_send (&msg, self->backend);
}

static gboolean check_socket_activity(GIOChannel *source, GIOCondition condition, GPPQueue *self)
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
      handle_backend (self);
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
      handle_frontend (self);
    }
  } while (go_on);

  return 1;
}

/* Heartbeating */

static void
send_heartbeat (gchar *id_string, Worker *worker, GPPQueue *self)
{
  zframe_send (&worker->identity, self->backend,
      ZFRAME_REUSE + ZFRAME_MORE);
  zframe_t *frame = zframe_new (PPP_HEARTBEAT, 1);
  zframe_send (&frame, self->backend, 0);
  g_debug ("sent heartbeat to one worker\n");
}

static gboolean
do_heartbeat (GPPQueue *self)
{
  g_hash_table_foreach (self->workerz, (GHFunc) send_heartbeat, self);

  g_debug ("doing heartbeat\n");
  purge_workers (self);
  return TRUE;
}

/* Initialization */

static void
create_channels (GPPQueue *self)
{
  self->ctx = zctx_new ();
  self->frontend = zsocket_new (self->ctx, ZMQ_ROUTER);
  self->backend = zsocket_new (self->ctx, ZMQ_ROUTER);
  zsocket_bind (self->frontend, "tcp://*:5555");
  zsocket_bind (self->backend,  "tcp://*:5556");

  self->frontend_channel = g_io_channel_from_zmq_socket (self->frontend);
  self->backend_channel = g_io_channel_from_zmq_socket (self->backend);
}

/* GObject */

static void
dispose (GObject *object)
{
  GPPQueue *self = GPP_QUEUE (object);

  g_hash_table_unref (self->workerz);
  zctx_destroy (&self->ctx);
}

static void
gpp_queue_class_init (GPPQueueClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = dispose;
}

static void
gpp_queue_init (GPPQueue *self)
{
  create_channels (self);

  self->workerz = g_hash_table_new_full (g_str_hash, g_str_equal,
      NULL, (GDestroyNotify) worker_destroy);
  self->available_workerz = g_queue_new();
}

/* API */

/**
 * gpp_queue_new:
 *
 * Create a new #GPPQueue, which doesn't yet listen to #GPPWorker (s).
 * Start it with gpp_queue_start()
 *
 * Returns: the newly-created #GPPQueue.
 */
GPPQueue *
gpp_queue_new (void)
{
  return g_object_new (GPP_TYPE_QUEUE, NULL);
}

/**
 * gpp_queue_start:
 * @self: A #GPPQueue to start.
 *
 * Makes @self start to route requests to available workers,
 * and check worker's liveness.
 *
 * Returns: %TRUE if the queue was started, %FALSE if it was already.
 */
gboolean
gpp_queue_start (GPPQueue *self)
{
  if (self->backend_source)
    return FALSE;

  self->backend_source = g_io_add_watch (self->backend_channel,
      G_IO_IN, (GIOFunc) check_socket_activity, self);

  g_timeout_add (HEARTBEAT_INTERVAL / 1000, (GSourceFunc) do_heartbeat, self);

  return TRUE;
}
