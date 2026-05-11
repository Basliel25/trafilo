#ifndef DISPATCHER_H
#define DISPATCHER_H
#include "bounded_queue.h"
#include "hashmap.h"

/**
 * @brief A Dispatcher type with user set configs
 */
typedef struct dispatcher_t {
    // Data Fields
    bounded_queue_t *bounded_q; /* The bounded queue populated by events*/
    hashmap_t *hash_m; 
    
    // User callback functions
    trafilo_parse_fn parse;
    trafilo_handle_fn handle;
    trafilo_event_free_fn event_free;

    // Thread pools
    size_t num_workers;
    pthread_t **threads;

    // Lifecycle
    int done; /* Shutdown signaling*/
    
} dispatcher_t;
#endif
