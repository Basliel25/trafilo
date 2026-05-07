#include "headers/bounded_queue.h"


bounded_queue_t *bq_create(size_t capacity) {
    bounded_queue_t *bounded_q;
    
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
void bq_destroy(bounded_queue_t *q);

int bq_push(bounded_queue_t *q, void *item);    
void *bq_pop(bounded_queue_t *q);                
void bq_shutdown(bounded_queue_t *q);            

