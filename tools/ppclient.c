//  Lazy Pirate client
//  Use zmq_poll to do a safe request-reply
//  To run, start lpserver and then randomly kill/restart it

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
task_done (GPPClient *client, const gchar *reply, gboolean success, gpointer unused)
{
  if (!success)
    g_print ("task failed\n");
  else
    g_print ("task succeeded : %s\n", reply);
  gpp_client_send_request (client, make_new_task (), -1, task_done, NULL);
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
  gpp_client_send_request (client, make_new_task(), -1, task_done, NULL);
  g_main_loop_run (loop);
  g_object_unref (client);
  return 0;
}
