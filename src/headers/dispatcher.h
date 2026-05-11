#ifndef DISPATCHER_H
#define DISPATCHER_H
#include "bounded_queue.h"
#include "hashmap.h"

/**
 * @brief A Dispatcher type with user set configs
 */
typedef struct dispatcher_t {
    bounded_queue_t *bounded_q; /* Working bounded queue*/
    hashmap_t *hash_m; /* Events map*/
    
    // User callback functions
    trafilo_parse_fn parse;
    trafilo_handle_fn handle;
    trafilo_event_free_fn event_free;

    // Thread pools
    size_t num_workers; /* Number of wokrer threads, fetched from trafilo_conifg_t*/
    pthread_t *threads; /* Array of worker thread pool*/

    // Lifecycle
    volatile int done; /* Shutdown signaling*/
    int started; 
} dispatcher_t;

/**
 * @brief Create a dispatcher thread
 * @param bounded_queue_t the working queue
 * @param hashmap_t the event map
 * @param trafilo_config_t the user set config,
 *        includes the user set callback functions
 *        and the desired number of workers.
 * @return dispatcher_t dispatcher, NULL on creation failure
 */
dispatcher_t *dispatcher_create(bounded_queue_t *bounded_q, 
        hashmap_t *hashmap, 
        const trafilo_config_t *trafilo_config);
/*
 * @brief Start a dispatcher thread
 * @param dispatcher_t the dispatcher to be started
 * @return 0 on success, -1 on empty dispatcher, -2 on Impodence
 */
int dispatcher_start(dispatcher_t *dispatcher);

/*
 * @brief Stop a running dispatcher,
 *        wakes all listeners and producers,
 *        shutdowns working queue, and drains fields,
 *        merge all dispatcher threads. Sets started
 *        flag to 0.
 * @param dispatcher_t the dispatcher to be started
 */
void dispatcher_stop(dispatcher_t *dispatcher);

/**
 * @brief Free the dispatcher and free memory.
 * @param dispatcher_t the dispatcher to be destroyed
 */
void dispatcher_destroy(dispatcher_t *dispatcher);
#endif
