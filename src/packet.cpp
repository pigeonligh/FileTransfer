#include "packet.h"

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>

bool receive_packet(int32_t fd, packet *pack) {
    uint32_t len = recv(fd, pack, PACKET_HEADER_SIZE, 0);
    
    if (len == 0) {
        return false;
    }

    while (len < PACKET_HEADER_SIZE) {
        len += recv(fd, pack + len, PACKET_HEADER_SIZE - len, 0);
        if (len < 0) {
            return false;
        }
    }

    if (pack->length > sizeof(packet)) {
        return false;
    }

    while (len < pack->length) {
        len += recv(fd, pack + len, pack->length - len, 0);
        if (len < 0) {
            return false;
        }
    }

    return true;
}