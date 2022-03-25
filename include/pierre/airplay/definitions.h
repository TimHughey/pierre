#ifndef _DEFINITIONS_H
#define _DEFINITIONS_H

#include <sys/socket.h>

#include "config.h"

// struct sockaddr_in6 is bigger than struct sockaddr. derp
#ifdef AF_INET6
#define SOCKADDR struct sockaddr_storage
#define SAFAMILY ss_family
#else
#define SOCKADDR struct sockaddr
#define SAFAMILY sa_family
#endif

#endif // _DEFINITIONS_H
