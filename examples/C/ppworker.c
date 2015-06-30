#include <glib-unix.h>

#include "gpp.h"

#define FAILURE_ODDS 4

#define GPP_TYPE_MULTIPLYING_WORKER (gpp_multiplying_worker_get_type ())

G_DECLARE_FINAL_TYPE (GPPMultiplyingWorker, gpp_multiplying_worker, GPP,
    MULTIPLYING_WORKER, GPPWorker);

struct _GPPMultiplyingWorker
{
  GPPWorker parent;
  GRand *rand_source;
  gchar *reply;
};

G_DEFINE_TYPE (GPPMultiplyingWorker, gpp_multiplying_worker, GPP_TYPE_WORKER);

static gboolean
set_task_done (GPPMultiplyingWorker * self)
{
  g_print ("one task done\n");
  if (g_rand_int (self->rand_source) % FAILURE_ODDS == 0) {
    g_print ("Actually it didn't work sorry\n");
    gpp_worker_set_task_done (GPP_WORKER (self), NULL, FALSE);
  } else {
    g_print ("no problem !!\n");
    gpp_worker_set_task_done (GPP_WORKER (self), self->reply, TRUE);
  }
  return FALSE;
}

static gboolean
handle_request (GPPWorker * worker, const gchar * request)
{
  GPPMultiplyingWorker *self = GPP_MULTIPLYING_WORKER (worker);
  g_print ("doing one task, request is %s\n", request);

  if (g_rand_int (self->rand_source) % FAILURE_ODDS == 0) {
    g_print ("I can't even start\n");
    return FALSE;
  }

  self->reply = g_strdup_printf ("Result : %d", atoi (request) * 2);
  g_timeout_add (1000, (GSourceFunc) set_task_done, self);
  return TRUE;
}

static void
gpp_multiplying_worker_class_init (GPPMultiplyingWorkerClass * klass)
{
  GPPWorkerClass *gpp_worker_class = GPP_WORKER_CLASS (klass);

  gpp_worker_class->handle_request = handle_request;
}

static void
gpp_multiplying_worker_init (GPPMultiplyingWorker * self)
{
  self->rand_source = g_rand_new ();
}

static gboolean
interrupted_cb (GMainLoop * loop)
{
  g_main_loop_quit (loop);
  return FALSE;
}

int
main (void)
{
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  GPPMultiplyingWorker *worker =
      g_object_new (GPP_TYPE_MULTIPLYING_WORKER, NULL);

  g_unix_signal_add_full (G_PRIORITY_HIGH, SIGINT, (GSourceFunc) interrupted_cb,
      loop, NULL);
  gpp_worker_start (GPP_WORKER (worker));

  g_main_loop_run (loop);
  g_object_unref (worker);
  return 0;
}
