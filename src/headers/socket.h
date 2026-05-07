#ifndef SOCKET_H
#define SOCKET_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "bounded_queue.h"

/**
 * @brief  listener handle.
 */
typedef struct listener_t {
    int sockfd; /* UDP Port */
    bounded_queue_t *bounded_q; /* Work queue associated with queue */
    size_t max_line; /* The max lines work queue can handel*/
    pthread_t thread; /* Listening and populating thread */
    int started; /* Operation flag */
    volatile int done; /* Atomic flag, for listener's thread*/
} listener_t;

/**
 * @brief Allocate a listener bound to the given UDP port.
 * @param port      UDP port to bind on 
 * @param bounded_q         queue raw lines are pushed onto.
 *                  Listener does NOT take ownership — caller manages lifetime.
 * @return  listener handle on success, NULL on socket/bind/alloc failure.
 */
listener_t *listener_create(int port, bounded_queue_t *bounded_q, size_t max_line);

/**
 * @brief Spawn the receive thread. 
 * @param listener  listener to start
 * @return   0 on success, -1 if pthread_create failed or already started
 */
int listener_start(listener_t *listener);

/**
 * @brief Signal the receive thread to exit and join it.
 *        Does NOT shut down the queue — caller decides when to call
 *        bq_shutdown.
 *
 * @param listener  listener to stop
 */
void listener_stop(listener_t *l);

/**
 * @brief Free the listener and close its socket.
 * @param listener listener to destroy
 */
void listener_destroy(listener_t *listener);
#endif
