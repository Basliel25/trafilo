#include "headers/socket.h"

static void *listener_loop(void *arg);

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

    listener->done = 0;

    return listener;
}

int listener_start(listener_t *listener) {
    // Return 
    if(listener == NULL) return -1;
    if(listener->started == 1) return -2;

    // Thread creation 
    if(pthread_create(&listener->thread, NULL, listener_loop, listener) != 1) 
        return -1;

    listener->started = 1;
    return 0;
}


void listener_stop(listener_t *listener) {
    if(listener == NULL) return;
    if(listener->started != 1) return;

    listener->done = 1;
    pthread_join(listener->thread, NULL);
    listener->started = 0;
}
void listener_destroy(listener_t *listener){
    if(listener == NULL) return;
    if(listener->started) listener_stop(listener);

    // Shutdown socket
    close(listener->sockfd);

    //Free listener heap
    free(listener);
    
}
