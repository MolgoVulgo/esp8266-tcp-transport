#ifndef TEST_LWIP_SOCKETS_H
#define TEST_LWIP_SOCKETS_H

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef ioctlsocket
#define ioctlsocket ioctl
#endif

#endif
