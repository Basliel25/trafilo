/**
 * Hashmap to store events
 * array of N buckets
 * each bucket:
 *  - key
 *  - user_states
 *  - sliding_window for sampling
 *  - mutex protected
 * chaining for collisions
 * FNV1-a hasing used for key (service) names
 */
#ifndef HASMAP_H
#define HASMAP_H

#include <pthread.h>
#include <time.h>
#include "window.h"
#include "../../include/trafilo.h"

/**
 * @brief A bucket node of the hashtable
 */
typedef struct bucket_node {
    char *key; /* Bucket owned key*/
    void *state; /* User defined state */
    sliding_window_t window; /* An embedded sliding window */

    pthread_mutex_t bucket_lock; /* Mutex lock for bucket */

    struct bucket_node_t *next;
    struct timespec last_event_ts;
} bucket_node;

/**
 * @brief The hasmap.
 */
typedef struct hashmap_t {
    bucket_node **buckets; /* Array of chain buckets */
    size_t num_buckets; /* Number of buckets in hasmap */
} hashmap_t;

/**
 * @brief Create a hashmap
 * @param size_t num_buckets: Number of buckets
 */
hashmap_t *hasmap_create(size_t num_buckets);

/**
 * @brief Graceful Cleanup
 * @param hasmap_t * pointer to the hashmap
 * @param trafilo_state_free_fn user specified state_free function
 */
void hasmap_destroy(hashmap_t *hashmap, trafilo_state_free_fn state_free);

/**
 * @brief For each iteration for operations
 *        calls passed function within lock protection
 * @param hasmap_t * pointer to the hashmap
 * @param void (*fn) function with bucket_node and arguments
 * @param void *arg arguments for the iterative function
 */
void hashmap_for_each(hashmap_t *hashmap, 
        void (*fn)(bucket_node *bucket, void *arg), 
        void *arg);

#endif
