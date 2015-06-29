//  Lazy Pirate client
//  Use zmq_poll to do a safe request-reply
//  To run, start lpserver and then randomly kill/restart it

#include <glib-unix.h>
#include "gpp.h"

static void
task_done (GPPClient *client, gboolean success, gpointer unused)
{
  gpp_client_send_request (client, "{plop: shit}", 2, task_done, NULL);
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
  gpp_client_send_request (client, "{plop: shit}", 2, task_done, NULL);
  g_main_loop_run (loop);
  g_object_unref (client);
  return 0;
}
