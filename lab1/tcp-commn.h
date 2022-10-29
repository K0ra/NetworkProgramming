#ifndef TCP_COMMN_H
#define TCP_COMMN_H

#include <sys/socket.h>

ssize_t tcp_recv(int sockfd, char *buf, size_t len) {
    size_t buf_ptr = 0;
    while (buf_ptr < len) {
        int rc = recv(sockfd, buf + buf_ptr, len - buf_ptr, 0);
        if (rc <= 0) {
            return -1;
        }
        buf_ptr += rc;
    }
    return 0;
}

ssize_t tcp_send(int sockfd, char *buf, size_t len) {
    size_t buf_ptr = 0;
    while (buf_ptr < len) {
        int rc = send(sockfd, buf + buf_ptr, len - buf_ptr, 0);
        if (rc <= 0) {
            return -1;
        }
        buf_ptr += rc;
    }
    return 0;
}

#endif