#include "headers/socket.h"


listener_t *listener_create(int port, bounded_queue_t *bounded_q, size_t max_line) {
    // Create a listener based on specifications
    listener_t *listener = malloc(sizeof(listener_t));
    if(listener == NULL) return NULL;

    // Create socket
    listener->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(listener->sockfd < 0) {
        free(listener);
        return NULL;
    }
    
    // Socket Address specifications
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    // If binding to the port fails free socket and return
    if (bind(listener->sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(listener->sockfd);
        free(listener);
        return NULL;
    }

    // Timeout check
    struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 };
    if (setsockopt(listener->sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        close(listener->sockfd);
        free(listener);
        return NULL;
    }

    listener->bounded_q = bounded_q;
    listener->max_line = max_line;

    listener->started = 1;
    listener->done = 0;

    return listener;
}

int listener_start(listener_t *listener);
void listener_stop(listener_t *l);
void listener_destroy(listener_t *listener);
