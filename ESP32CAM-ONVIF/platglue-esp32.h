#pragma once

#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

typedef WiFiClient *SOCKET;
typedef WiFiUDP *UDPSOCKET;
typedef IPAddress IPADDRESS;
typedef uint16_t IPPORT;

#define NULLSOCKET NULL

inline void closesocket(SOCKET s) {
    if (s) {
        s->stop();
        delete s;
    }
}

#define getRandom() random(65536)

inline void socketpeeraddr(SOCKET s, IPADDRESS *addr, IPPORT *port) {
    if (!s) return;
    *addr = s->remoteIP();
    *port = s->remotePort();
}

inline void udpsocketclose(UDPSOCKET s) {
    if (s) {
        s->stop();
        delete s;
    }
}

inline UDPSOCKET udpsocketcreate(unsigned short portNum)
{
    UDPSOCKET s = new WiFiUDP();
    if (!s->begin(portNum)) {
        printf("Can't bind UDP port %d\n", portNum);
        delete s;
        return NULL;
    }
    return s;
}

// TCP sending — guards against null and disconnected sockets
inline ssize_t socketsend(SOCKET sockfd, const void *buf, size_t len)
{
    if (!sockfd || !sockfd->connected()) return 0;
    return sockfd->write((uint8_t *)buf, len);
}

// UDP sending — guards against null socket
inline ssize_t udpsocketsend(UDPSOCKET sockfd, const void *buf, size_t len,
                             IPADDRESS destaddr, IPPORT destport)
{
    if (!sockfd) return 0;
    sockfd->beginPacket(destaddr, destport);
    sockfd->write((const uint8_t *)buf, len);
    if (!sockfd->endPacket())
        printf("error sending udp packet\n");
    return len;
}

/**
   Read from a socket with a timeout.
   Return 0=socket was closed by client, -1=timeout, >0 number of bytes read
 */
inline int socketread(SOCKET sock, char *buf, size_t buflen, int timeoutmsec)
{
    if (!sock || !sock->connected()) {
        return 0;  // Client disconnected
    }

    int numAvail = sock->available();
    if (numAvail == 0 && timeoutmsec != 0) {
        delay(timeoutmsec);
        numAvail = sock->available();
    }

    if (numAvail == 0) {
        return -1;  // Timeout
    }

    int numRead = sock->readBytes(buf, buflen);
    return numRead;
}
