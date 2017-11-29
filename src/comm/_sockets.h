/*
 * Socket types and error definitions.
 */
#ifdef HAVE_WINSOCK2_H
#include <Winsock2.h>
#include <windows.h>
#define OC_SOCKET_ERROR      SOCKET_ERROR
#define OC_INVALID_SOCKET    INVALID_SOCKET
/* typedef SOCKET CASocketFd_t; */
#define CASocketFd_t SOCKET

#else // not HAVE_WINSOCK2_H
#define OC_SOCKET_ERROR      (-1)
#define OC_INVALID_SOCKET    (-1)
#define POSIX_SOCKET_ERROR      (-1)
#define POSIX_FCNTL_ERROR      (-1)
#define POSIX_SETSOCKOPT_ERROR      (-1)
#define POSIX_GETSOCKNAME_ERROR      (-1)
/* typedef int    CASocketFd_t; */
#define CASocketFd_t int
#endif

/**
 * Hold global variables for CA layer. (also used by RI layer)
 */
typedef struct
{
    CASocketFd_t fd;    /**< socket fd */
    uint16_t port;      /**< socket port */
} CASocket_t;