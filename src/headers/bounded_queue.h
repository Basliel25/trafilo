#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>

/**
 * @brief A Circular ring queue, fixed size,
 *        to create a pool of events to be parsed.
 */
typedef struct {
    void **buf; /* ring of capacity slots*/
    size_t capacity;
    size_t head; /* next slot to pop from*/
    size_t tail; /* next slot to push into*/
    size_t count; /* current occupancy */

    pthread_mutex_t mu_lock;
    pthread_cond_t not_empty; /*A conditional signal when queue is empty, blocks/frees consumers*/
    pthread_cond_t not_full; /*A conditional signal when queue is full, blocks/frees producers*/

    int done; /*Shutdown flag*/
} bounded_queue_t;

/**
 * @brief Create a bounded queue with a fixed capacity.
 * @param capacity The maximum number of elements the queue can hold.
 * @return Pointer to a bounded_queue_t, NULL on bad-capacity or alloc failure.
 */
bounded_queue_t *bq_create(size_t capacity);

/**
 * @brief Destroy the bounded queue and free resources.
 * @param q The bounded queue to destroy.
 */
void bq_destroy(bounded_queue_t *q);

/**
 * @brief Add an entry to the queue. Handles not_full, blocks while full.
 * @param q The bounded queue.
 * @param item The item to be added.
 * @return 0 on success, -1 if shutdown is called while waiting on not_full.
 */
int bq_push(bounded_queue_t *q, void *item);    

/**
 * @brief Pop an entry from the queue. Handles not_empty signal, blocks if empty.
 * @param q The bounded queue.
 * @return Pointer to the popped item.
 */
void *bq_pop(bounded_queue_t *q);                

/**
 * @brief Handle clean shutdown when working queue is done.
 *        Responsible for handling all waiting workers on dead signals
 *        and setting the shutdown flag.
 * @param q The bounded queue.
 */
void bq_shutdown(bounded_queue_t *q);            
#endif
