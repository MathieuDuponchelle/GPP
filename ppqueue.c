//  Paranoid Pirate queue

#include <glib.h>
#include <glib-unix.h>
#include <gio/gio.h>
#include "czmq.h"
#define HEARTBEAT_LIVENESS  3       //  3-5 is reasonable
#define HEARTBEAT_INTERVAL  1000    //  msecs

//  Paranoid Pirate Protocol constants
#define PPP_READY       "\001"      //  Signals worker is ready
#define PPP_HEARTBEAT   "\002"      //  Signals worker heartbeat

//  .split worker class structure
//  Here we define the worker class; a structure and a set of functions that
//  act as constructor, destructor, and methods on worker objects:

typedef struct {
    zframe_t *identity;         //  Identity of worker
    char *id_string;            //  Printable identity
    int64_t expiry;             //  Expires at this time
} worker_t;

//  Construct new worker
static worker_t *
s_worker_new (zframe_t *identity)
{
    worker_t *self = (worker_t *) zmalloc (sizeof (worker_t));
    self->identity = identity;
    self->id_string = zframe_strhex (identity);
    self->expiry = zclock_time ()
                 + HEARTBEAT_INTERVAL * HEARTBEAT_LIVENESS;
    return self;
}

//  Destroy specified worker object, including identity frame.
static void
s_worker_destroy (worker_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        worker_t *self = *self_p;
        zframe_destroy (&self->identity);
        free (self->id_string);
        free (self);
        *self_p = NULL;
    }
}

//  .split worker ready method
//  The ready method puts a worker to the end of the ready list:

static void
s_worker_ready (worker_t *self, zlist_t *workers)
{
    worker_t *worker = (worker_t *) zlist_first (workers);
    while (worker) {
        if (streq (self->id_string, worker->id_string)) {
            zlist_remove (workers, worker);
            s_worker_destroy (&worker);
            break;
        }
        worker = (worker_t *) zlist_next (workers);
    }
    zlist_append (workers, self);
}

//  .split get next available worker
//  The next method returns the next available worker identity:

static zframe_t *
s_workers_next (zlist_t *workers)
{
    worker_t *worker = zlist_pop (workers);
    assert (worker);
    zframe_t *frame = worker->identity;
    worker->identity = NULL;
    s_worker_destroy (&worker);
    return frame;
}

//  .split purge expired workers
//  The purge method looks for and kills expired workers. We hold workers
//  from oldest to most recent, so we stop at the first alive worker:

static void
s_workers_purge (zlist_t *workers)
{
    worker_t *worker = (worker_t *) zlist_first (workers);
    while (worker) {
        if (zclock_time () < worker->expiry)
            break;              //  Worker is alive, we're done here

        zlist_remove (workers, worker);
        s_worker_destroy (&worker);
        worker = (worker_t *) zlist_first (workers);
    }
}

//  .split main task
//  The main task is a load-balancer with heartbeating on workers so we
//  can detect crashed or blocked worker tasks:

typedef struct {
  void *frontend;
  void *backend;
  zlist_t *workers;
  GMainLoop *loop;
  GIOChannel *frontend_channel;
  guint frontend_source;
  uint64_t heartbeat_at;
} ppqueue_t;

static gboolean callback_func(GIOChannel *source, GIOCondition condition, ppqueue_t *self);

int s_handle_backend (ppqueue_t *self)
{
  zmsg_t *msg = zmsg_recv (self->backend);
  if (!msg) {
    return -1;
  }

  //  Any sign of life from worker means it's ready
  zframe_t *identity = zmsg_unwrap (msg);
  worker_t *worker = s_worker_new (identity);
  s_worker_ready (worker, self->workers);

  if (zlist_size (self->workers) == 1 && !self->frontend_source) {
    self->frontend_source = g_io_add_watch(self->frontend_channel,
        G_IO_IN, (GIOFunc) callback_func, self);
  }

  //  Validate control message, or return reply to client
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
    zmsg_send (&msg, self->frontend);
  }
  return 0;
}

int s_handle_frontend (ppqueue_t *self)
{
  zmsg_t *msg = zmsg_recv (self->frontend);
  if (!msg) {
    return -1;
  }
  zframe_t *identity = s_workers_next (self->workers); 
  if (zlist_size (self->workers) == 0) {
    g_source_remove (self->frontend_source);
    self->frontend_source = 0;
  }
  zmsg_prepend (msg, &identity);
  zmsg_send (&msg, self->backend);
  return 0;
}

static gboolean callback_func(GIOChannel *source, GIOCondition condition, ppqueue_t *self)
{
  uint32_t status;
  size_t sizeof_status = sizeof(status);
  gboolean go_on;

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

  return 1; // keep the callback active
}

static gboolean
do_heartbeat (ppqueue_t *self)
{
  worker_t *worker = (worker_t *) zlist_first (self->workers);

  while (worker) {
    zframe_send (&worker->identity, self->backend,
        ZFRAME_REUSE + ZFRAME_MORE);
    zframe_t *frame = zframe_new (PPP_HEARTBEAT, 1);
    zframe_send (&frame, self->backend, 0);
    worker = (worker_t *) zlist_next (self->workers);
  }

  s_workers_purge (self->workers);
  return TRUE;
}

static gboolean
interrupted_cb (ppqueue_t *self)
{
  g_main_loop_quit (self->loop);
  return FALSE;
}

int main (void)
{
  zctx_t *ctx = zctx_new ();
  ppqueue_t *self = g_malloc0 (sizeof (ppqueue_t));
  self->frontend = zsocket_new (ctx, ZMQ_ROUTER);
  self->backend = zsocket_new (ctx, ZMQ_ROUTER);
  zsocket_bind (self->frontend, "tcp://*:5555");    //  For clients
  zsocket_bind (self->backend,  "tcp://*:5556");    //  For workers
  self->loop = g_main_loop_new (NULL, FALSE);

  //  List of available workers
  self->workers = zlist_new ();

  {
    int fd;
    size_t sizeof_fd = sizeof(fd);
    if(zmq_getsockopt(self->frontend, ZMQ_FD, &fd, &sizeof_fd))
      perror("retrieving zmq fd");
    self->frontend_channel = g_io_channel_unix_new(fd);
  }

  {
    int fd;
    size_t sizeof_fd = sizeof(fd);
    if(zmq_getsockopt(self->backend, ZMQ_FD, &fd, &sizeof_fd))
      perror("retrieving zmq fd");
    GIOChannel* channel = g_io_channel_unix_new(fd);
    g_io_add_watch(channel, G_IO_IN, (GIOFunc) callback_func, self);
  }

  g_unix_signal_add_full (G_PRIORITY_DEFAULT, SIGINT, (GSourceFunc) interrupted_cb, self, NULL);
  g_timeout_add (HEARTBEAT_INTERVAL, (GSourceFunc) do_heartbeat, self);
  g_main_loop_run (self->loop);
  //  When we're done, clean up properly
  while (zlist_size (self->workers)) {
    worker_t *worker = (worker_t *) zlist_pop (self->workers);
    s_worker_destroy (&worker);
  }
  zlist_destroy (&self->workers);
  zctx_destroy (&ctx);
  return 0;
}
