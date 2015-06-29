#include <czmq.h>

#include "gpputils.h"
  
GIOChannel *
g_io_channel_from_zmq_socket (void *socket)
{
  int fd;
  size_t sizeof_fd = sizeof(fd);
  if (zmq_getsockopt(socket, ZMQ_FD, &fd, &sizeof_fd))
    perror("retrieving zmq fd");

  return g_io_channel_unix_new(fd);
}
