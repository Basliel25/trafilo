#include "headers/bounded_queue.h"
#include <pthread.h>

bounded_queue_t *bq_create(size_t capacity) {
    bounded_queue_t *bounded_q;
    
    if(capacity == 0) return NULL;

    bounded_q = malloc(sizeof(bounded_queue_t));
    if(bounded_q == NULL) {
        free(bounded_q);
        return NULL;
    }

    bounded_q->buf = malloc(sizeof(void *) * capacity);
    if(bounded_q->buf == NULL) {
        free(bounded_q->buf);
        return NULL;
    }

    bounded_q->capacity = capacity;
    bounded_q->head = 0;
    bounded_q->tail = 0;
    bounded_q->count = 0;

    pthread_mutex_init(&bounded_q->mu_lock, NULL);
    pthread_cond_init(&bounded_q->not_empty, NULL);
    pthread_cond_init(&bounded_q->not_full, NULL);

    bounded_q->done = 0;
    return bounded_q;
}

void bq_destroy(bounded_queue_t *q) {
    pthread_mutex_destroy(&q->mu_lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    
    free(q->buf);
    free(q);
}

int bq_push(bounded_queue_t *q, void *item) {
    pthread_mutex_lock(&q->mu_lock);
    //If full wait for not full signal to push
    while(!q->done && q->count == q->capacity) {
        pthread_cond_wait(&q->not_full, &q->mu_lock);
    }
    
    // If done is flagged, unlock and return 
    if(q->done) {
        pthread_mutex_unlock(&q->mu_lock);
        return -1;
    }

    // If pushing is still valid, append 
    q->buf[q->tail] = item;
    q->tail = (q->tail + 1) % q->capacity;
    q->count += 1;
    pthread_cond_signal(&q->not_empty);

    pthread_mutex_unlock(&q->mu_lock);

    return 0;
}

void *bq_pop(bounded_queue_t *q){

    pthread_mutex_lock(&q->mu_lock);
    while(q->count == 0 && !q->done) {
        pthread_cond_wait(&q->not_empty, &q->mu_lock);
    }
    if(q->count == 0) {
        pthread_mutex_unlock(&q->mu_lock);
        return NULL;
    }

    void *item = q->buf[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count -= 1;

    pthread_cond_signal(&q->not_full);

    pthread_mutex_unlock(&q->mu_lock);
    return item;
}

void bq_shutdown(bounded_queue_t *q) {
    // Acqure lock and set flag
    pthread_mutex_lock(&q->mu_lock);
    if(q->done) {
        pthread_mutex_unlock(&q->mu_lock);
    }
    q->done = 1;

    // Wake up all listening workers
    pthread_cond_broadcast(&q->not_empty);
    
    // Wake up all listening producers
    pthread_cond_broadcast(&q->not_empty);

    pthread_mutex_unlock(&q->mu_lock);
}            

