#ifndef HASHMAP_H
#define HASHMAP_H

#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "window.h"
#include "../../include/trafilo.h"

/**
 * @brief A bucket node of the hashtable.
 */
typedef struct bucket_node {
    char *key; /* Bucket owned key*/
    void *state; /* User defined state */
    sliding_window_t *window; /* An embedded sliding window */

    struct bucket_node *next;
    struct timespec last_event_ts;
} bucket_node;

/**
 * @brief The hashmap structure.
 */
typedef struct hashmap_t {
    bucket_node **buckets; /* Array of chain buckets */
    pthread_mutex_t *locks; /* Array of per bucket locks*/
    size_t num_buckets; /* Number of buckets in hashmap */
} hashmap_t;

/**
 * @brief Create a hashmap.
 * @param num_buckets Number of buckets in the hashmap.
 * @return Pointer to the newly created hashmap.
 */
hashmap_t *hashmap_create(size_t num_buckets);

/**
 * @brief Find bucket for key; create if absent. Returns with bucket->bucket_lock LOCKED.
 * @param hashmap Target hashmap.
 * @param key NUL-terminated string, bucket takes ownership.
 * @return Locked bucket, or NULL on alloc failure.
 */
bucket_node *hashmap_find_or_create(hashmap_t *hashmap, const char *key);

/**
 * @brief Graceful cleanup of the hashmap.
 * @param hashmap Pointer to the hashmap.
 * @param state_free User specified state_free function.
 */
void hashmap_destroy(hashmap_t *hashmap, trafilo_state_free_fn state_free);

/**
 * @brief For each iteration for operations, calls passed function within lock protection.
 * @param hashmap Pointer to the hashmap.
 * @param fn Function with bucket_node and arguments.
 * @param arg Arguments for the iterative function.
 */
void hashmap_for_each(hashmap_t *hashmap, 
        void (*fn)(bucket_node *bucket, void *arg), 
        void *arg);

/**
 * @brief Helper to unlock a locked node.
 * @param hashmap Pointer to the hashmap.
 * @param key The key that belongs to the node.
 */
void hashmap_unlock_bucket(hashmap_t *hashmap, const char *key);

#endif
