#include "../include/trafilo.h"
#include "headers/bounded_queue.h"
#include <time.h>
#include "headers/dispatcher.h"

static void *dispatcher_loop(void *arg){
    dispatcher_t *dispatcher = (dispatcher_t *) arg;
    const trafilo_config_t *config = dispatcher->config;

    for(;;) {
        // Pop a line from the queue
        char *raw_line = bq_pop(dispatcher->bounded_q);
        if(raw_line == NULL) 
            break;
        

        size_t line_len = strlen(raw_line);

        // Parsing Phase
        event_t *event = NULL;
        int rc = config->parse(raw_line, line_len, &event);

        if(rc != 0) {
            // Parse rejected the line
            // No event assigned
            free(raw_line);
            continue;
        }

        // Window add?

        // Sanity check if key is set as NULL
        if(event->key == NULL) {
            config->event_free(event);
            free(raw_line);
            continue;
        }

        // Lookup and bucket creation
        bucket_node *bucket = hashmap_find_or_create(dispatcher->hash_m, event->key);
        if(bucket == NULL) {
            // Internal hashmap failure
            // No bucket created and no event assigned
            config->event_free(event);
            free(raw_line);
            continue;
        }

        // Bucket node created succesfully
        // State initialization
        // !! If state init is null, the user should handle it
        if(bucket->state == NULL && config->state_init != NULL) {
            //Initalize and attach state to bucket
            bucket->state = config->state_init(event->key);
        }
        if(bucket->window == NULL) {
            sliding_window_init(bucket->window,
                    config->window_size_ms,
                    config->slide_interval_ms);
        }

        // Handle event according to user specification
        config->handle(event, bucket->state);
        sliding_window_add(bucket->window, event->t_secs);

        clock_gettime(CLOCK_MONOTONIC, &bucket->last_event_ts);

        // If window should emit
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        int should_emit = sliding_window_should_emit(bucket->window, now);
        void *state_snapshot = NULL;
        window_result_t window;
        if(should_emit){
            window_result_t window = { bucket->key, 
                bucket->window->count, 
                sliding_window_oldest(bucket->window),
                sliding_window_newest(bucket->window)};
            state_snapshot = bucket->state;
            sliding_window_mark_emitted(bucket->window, now);

        }

        hashmap_unlock_bucket(dispatcher->hash_m, bucket->key);

        if(should_emit && config->sink != NULL)
            config->sink(bucket->key, &window, bucket->state);

        config->event_free(event);
        free(raw_line);
    }
    return NULL;
}

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
    // Attach caller owned config struct to dispatch
    dispatcher->config = trafilo_config;

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
    bq_shutdown(dispatcher->bounded_q);
    for(size_t i = 0; i < dispatcher->num_workers; i++) {
        pthread_join(dispatcher->threads[i], NULL);
    }

    dispatcher->started = 0;
}

void dispatcher_destroy(dispatcher_t *dispatcher) {
    if(dispatcher == NULL) return;

    // If dispatcher is not done terminate destroy
    if(dispatcher->started) dispatcher_stop(dispatcher);

    free(dispatcher->threads);
    free(dispatcher);
}
