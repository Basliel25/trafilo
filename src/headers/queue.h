#ifndef QUEUE_H
#define QUEUE_H
#include <stddef.h>
#include <pthread.h>

/**
 * @brief a Circular ring queue, fixed size,
 *          to create a pool of events to be parsed
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
 * @brief A function to create a bounded queue
 *        with a fixed capacity
 * @param size_t capacity
 * @return a bounded_queue_t, NULL on bad-capacity or alloc failure
 */
bounded_queue_t *bq_create(size_t capacity);

/**
 * @brief A function to destroy the bounded queue
 */
void bq_clean(bounded_queue_t *q);

/**
 * @brief Add an entry to the queue,
 *        Handles not_full, blocks while full
 * @param bounded_queue_t q
 * @param void *item to be added
 * @return 0 on success, -1 if shutdown is called while waining on not_full
 */
int  bq_push(bounded_queue_t *q, void *item);    

/**
 * @brief pop entry from the queue, on entry parsing
 *        Handles the not_empty signal, blocks if empty
 * @param bounded_queue_t
 */
void *bq_pop(bounded_queue_t *q);                
/**
 * @brief Function to handle clean shutdown when working queue is done
 * Responsible to handle all waiting workers on dead signals
 * Sets shutdown flag as well.
 */
void bq_shutdown(bounded_queue_t *q);            
#endif
