#ifndef _PUBSUB_NET_HEADER
#define _PUBSUB_NET_HEADER

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

#pragma comment (lib, "Ws2_32.lib")
#elif ESP32
#include <lwip/sockets.h>
#else
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#endif
#endif