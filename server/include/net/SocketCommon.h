#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using SOCKET_TYPE = SOCKET;
const SOCKET_TYPE INVALID_SOCKET_VALUE = INVALID_SOCKET;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
using SOCKET_TYPE = int;
const SOCKET_TYPE INVALID_SOCKET_VALUE = -1;
#endif
