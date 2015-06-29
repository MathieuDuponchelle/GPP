#include <glib-unix.h>

#include "gpp.h"
#include "czmq.h"

static gboolean
interrupted_cb (GMainLoop *loop)
{
  g_main_loop_quit (loop);
  return FALSE;
}

static gboolean
set_task_done (GPPWorker *worker)
{
  printf ("one task done\n");
  gpp_worker_set_task_done (worker, TRUE);
  return FALSE;
}

static gboolean
task_handler (GPPWorker *worker, gpointer unused)
{
  printf ("doing one task\n");
  g_timeout_add (1000, (GSourceFunc) set_task_done, worker);
  return TRUE;
}

int main (void)
{
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  GPPWorker *self = gpp_worker_new();

  g_unix_signal_add_full (G_PRIORITY_HIGH, SIGINT, (GSourceFunc) interrupted_cb, loop, NULL);
  gpp_worker_start (self, task_handler, NULL);

  g_main_loop_run (loop);
  g_object_unref (self);
  return 0;
}
