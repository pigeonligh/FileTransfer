#include "server.h"

#include "connection_info.h"
#include "keepalive.h"
#include "packet.h"
#include "event.h"
#include "lock.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>

void new_connection(const epoll_event &event);
void listen_from_connection(const epoll_event &event);

int32_t listen_fd = -1;

int32_t init_fd(int32_t port) {
    int32_t fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
    if (fd == -1) {
        perror("create socket");
        exit(EXIT_FAILURE);
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr = {INADDR_ANY};
    server_addr.sin_port = htons(port);
    if (bind(fd, (sockaddr*) &server_addr, sizeof server_addr) == -1) {
        perror("bind socket");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (listen(fd, MAX_CONNECTIONS) == -1) {
        perror("listen socket");
        close(fd);
        exit(EXIT_FAILURE);
    }

    add_event(fd);

    return fd;
}

void init(int32_t port) {
    init_event();

    listen_fd = init_fd(port);
    printf("server runs in *:%d\n", port);

    init_connections();
    keepalive_start();
}

void run_once() {
    epoll_event event;
    read_event(&event);
    printf("get event\n");
    if (event.data.fd == listen_fd) {
        new_connection(event);
    } else {
        listen_from_connection(event);
    }
}

void server_start(int32_t port) {
    init(port);
    while (true) {
        run_once();
    } 
}

void server_close() {
    close(listen_fd);
    keepalive_close();
    close_event();
    close_all_connections();
}

void new_connection(const epoll_event &event) {
    int32_t fd = event.data.fd;
    printf("create connection from %d\n", fd);

    sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int32_t connect_fd = accept(fd, (sockaddr*) &client_addr, &addr_len);

    if (connect_fd == -1) {
        printf("create connection failed.");
        return;
    }

    set_lock();

    connection_info *ci = new_connection(connect_fd);
    add_event(ci->fd);

    unset_lock();
}

void process_packet(connection_info* ci) {
    int32_t client_fd = ci->fd;

    packet pack;
    uint32_t len = 0;
    while (len < PACKET_HEADER_SIZE) {
        len += recv(client_fd, &pack + len, PACKET_HEADER_SIZE - len, 0);
        if (len < 0) {
            perror("receive from client");
            close_connection(client_fd);
            return;
        }
    }

    while (len < pack.length) {
        len += recv(client_fd, &pack + len, PACKET_HEADER_SIZE - len, 0);
        if (len < 0) {
            perror("receive from client");
            close_connection(client_fd);
            return;
        }
    }

    if (pack.type == TYPE_CONNECT) {
        // this is useless
    } else if (pack.type == TYPE_REQUEST) {
        // TODO: process request from client
        printf("receive request from client %d\n", client_fd);
    } else if (pack.type == TYPE_SEND) {
        // TODO: process send request from client
        printf("receive data from client %d\n", client_fd);
    } else if (pack.type == KEEPALIVE) {
        printf("keepalive for client %d\n", client_fd);
        ci->secs = time(nullptr);
    } else {
        printf("unknown type packet");
    }
}

void listen_from_connection(const epoll_event &event) {
    if (event.events & EPOLLIN) {
        int32_t fd = event.data.fd;
        printf("listen from %d\n", fd);
        connection_info *ci = find_connection(fd);
        if (ci != nullptr) {
            process_packet(ci);
        }
    }
}
