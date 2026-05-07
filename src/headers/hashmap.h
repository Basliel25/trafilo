#ifndef HASMAP_H
#define HASMAP_H

#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "window.h"
#include "../../include/trafilo.h"

/**
 * @brief A bucket node of the hashtable
 */
typedef struct bucket_node {
    char *key; /* Bucket owned key*/
    void *state; /* User defined state */
    sliding_window_t window; /* An embedded sliding window */

    struct bucket_node *next;
    struct timespec last_event_ts;
} bucket_node;

/**
 * @brief The hasmap.
 */
typedef struct hashmap_t {
    bucket_node **buckets; /* Array of chain buckets */
    pthread_mutex_t *locks; /* Array of per bucket locks*/
    size_t num_buckets; /* Number of buckets in hasmap */
} hashmap_t;

/**
 * @brief Create a hashmap
 * @param size_t num_buckets: Number of buckets
 */
hashmap_t *hasmap_create(size_t num_buckets);

/**
 * @brief Find bucket for key; create if absent. Returns with bucket->bucket_lock LOCKED.
 * @param hashmap target
 * @param key NUL-terminated string, bucket takes ownership 
 * @return locked bucket, or NULL on alloc failure
 */
bucket_node *hashmap_find_or_create(hashmap_t *hashmap, const char *key);

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
