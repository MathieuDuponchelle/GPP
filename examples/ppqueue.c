#include <glib-unix.h>

#include "gpp.h"

static gboolean
interrupted_cb (GMainLoop *loop)
{
  g_main_loop_quit (loop);
  return FALSE;
}

int main (void)
{
  GPPQueue *self = gpp_queue_new ();
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);

  g_unix_signal_add_full (G_PRIORITY_DEFAULT, SIGINT, (GSourceFunc) interrupted_cb, loop, NULL);
  gpp_queue_start (self);
  g_main_loop_run (loop);

  return 0;
}
