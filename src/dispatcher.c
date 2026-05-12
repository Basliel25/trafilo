#include "../include/trafilo.h"
#include "headers/bounded_queue.h"
#include "headers/dispatcher.h"

static void *dispatcher_loop(void *arg){return NULL;}

dispatcher_t *dispatcher_create(bounded_queue_t *bounded_q, 
        hashmap_t *hashmap, 
        const trafilo_config_t *trafilo_config) {
    
    dispatcher_t *dispatcher;
    dispatcher = malloc(sizeof(dispatcher_t));
    
    if(dispatcher == NULL) return NULL;

    if(bounded_q == NULL || hashmap == NULL) {
        free(dispatcher);
        return NULL;
    }
    // Set memory fileds
    dispatcher->bounded_q = bounded_q;
    dispatcher->hash_m = hashmap;

    if(trafilo_config == NULL) {
        free(dispatcher);
        return NULL;
    }
    // Set user callback functions
    dispatcher->parse = trafilo_config->parse;
    dispatcher->handle = trafilo_config->handle;
    dispatcher->event_free = trafilo_config->event_free;

    // Thread pool creation
    if(trafilo_config->num_workers == 0) {
        free(dispatcher);
        return NULL; 
    }
    dispatcher->num_workers = trafilo_config->num_workers;

    // Allocate memeory for threads
    dispatcher->threads = malloc(sizeof(pthread_t) * dispatcher->num_workers);
    if (dispatcher->threads == NULL) {
        free(dispatcher);
        return NULL;
    }

    dispatcher->done = 0;
    dispatcher->started = 0;
    return dispatcher;
}

int dispatcher_start(dispatcher_t *dispatcher) {
    if(dispatcher == NULL) return -1;

    if(dispatcher->started) return -2;

    // Spin worker threads
    for (size_t i = 0; i < dispatcher->num_workers; i++) {
        if (pthread_create(&dispatcher->threads[i], NULL, dispatcher_loop, dispatcher) != 0) {
            // If thread creation fails, rollback and
            // join previously created threads
            bq_shutdown(dispatcher->bounded_q);
            for (size_t j = 0; j < i; j++) {
                pthread_join(dispatcher->threads[j], NULL);
            }
            return -1;
        }
    }
    dispatcher->started = 1;
    return 0;
}
void dispatcher_stop(dispatcher_t *dispatcher) {
    if(dispatcher == NULL) return;

    if(!dispatcher->started) return;

    //Terminate work and join threads
    dispatcher->done = 1;
    bq_shutdown(dispatcher->bounded_q);
    for(size_t i = 0; i < dispatcher->num_workers; i++) {
        pthread_join(dispatcher->threads[i], NULL);
    }

    dispatcher->started = 0;
}

void dispatcher_destroy(dispatcher_t *dispatcher) {
    if(dispatcher == NULL) return;

    // If dispatcher is not done terminate destroy
    if(!dispatcher->done) dispatcher_stop(dispatcher);

    free(dispatcher->threads);
    free(dispatcher);
}
