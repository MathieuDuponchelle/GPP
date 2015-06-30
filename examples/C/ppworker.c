#include <glib-unix.h>

#include "gpp.h"
#include "czmq.h"

#define FAILURE_ODDS 4

static GRand *rand_source;

static gboolean
interrupted_cb (GMainLoop *loop)
{
  g_main_loop_quit (loop);
  return FALSE;
}

typedef struct
{
  gchar *reply;
  GPPWorker *worker;
} WorkerData;

static gboolean
set_task_done (WorkerData *data)
{
  printf ("one task done\n");
  if (g_rand_int (rand_source) % FAILURE_ODDS == 0) {
    printf ("Actually it didn't work sorry\n");
    gpp_worker_set_task_done (data->worker, NULL, FALSE);
  } else {
    printf ("no problem !!\n");
    gpp_worker_set_task_done (data->worker, data->reply, TRUE);
  }
  return FALSE;
}

static gboolean
task_handler (GPPWorker *worker, const gchar *request, WorkerData *data)
{
  printf ("doing one task, request is %s\n", request);

  if (g_rand_int (rand_source) % FAILURE_ODDS == 0) {
    printf ("I can't even start\n");
    return FALSE;
  }

  data->reply = g_strdup_printf ("Result : %d", atoi (request) * 2);
  g_timeout_add (1000, (GSourceFunc) set_task_done, data);
  return TRUE;
}

int main (void)
{
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  WorkerData *data = g_slice_new (WorkerData);
  data->worker = gpp_worker_new();

  rand_source = g_rand_new ();
  g_unix_signal_add_full (G_PRIORITY_HIGH, SIGINT, (GSourceFunc) interrupted_cb, loop, NULL);
  gpp_worker_start (data->worker, (GPPWorkerTaskHandler) task_handler, data);

  g_main_loop_run (loop);
  g_object_unref (data->worker);
  g_slice_free (WorkerData, data);
  return 0;
}
