#include <glib-unix.h>
#include "gpp.h"

static gchar *
make_new_task (void)
{
  static int sequence = 0;
  gchar *task = g_strdup_printf ("%d", sequence);
  sequence++;

  g_print ("Doing task %s\n", task);
  return task;
}

static void
task_done_cb (GPPClient *client, gboolean success, const gchar *reply, gpointer unused)
{
  if (!success)
    g_print ("task failed\n");
  else
    g_print ("task succeeded : %s\n", reply);
  gpp_client_send_request (client, make_new_task (), -1);
}

static gboolean
interrupted_cb (GMainLoop *loop)
{
  g_main_loop_quit (loop);
  return FALSE;
}

int main (void)
{
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  GPPClient *client = gpp_client_new ();

  g_unix_signal_add_full (G_PRIORITY_HIGH, SIGINT, (GSourceFunc) interrupted_cb, loop, NULL);
  g_signal_connect (client, "request-handled", G_CALLBACK (task_done_cb), NULL);
  gpp_client_send_request (client, make_new_task(), -1);
  g_main_loop_run (loop);
  g_object_unref (client);
  return 0;
}
