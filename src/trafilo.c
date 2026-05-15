#include "headers/bounded_queue.h"
#include "headers/hashmap.h"
#include "headers/window.h"
#include "headers/socket.h"
#include "headers/dispatcher.h"
#include "../include/trafilo.h"

#define QUEUE_CAPACITY 4096
#define MAX_LINE 2048

struct trafilo {
    trafilo_config_t config; /*< Owned copy of trafilo config*/
    char *bind_addr_config; /*< config.bind_add_config, I will strdup it*/

    bounded_queue_t *bounded_q ;
    hashmap_t *hash_m;
    listener_t *listener;
    dispatcher_t *dispatcher;

    // Thread control
    pthread_mutex_t shutdown_lock;
    pthread_cond_t shutdown_cond;

    // Flags
    int shutdown_flag;
    int running;
};

/**
 * @brief Validate the trafilo_config_t state
 * @param const trafilo_conifg_t
 * @return 0 on valid state, -1 on error
 */
static int validate_config(const trafilo_config_t *config) {
    if(config == NULL) return -1; /* Empty config*/
    if(config->parse == NULL ||
            config->handle == NULL ||
            config->event_free == NULL) return -1; /* Empty callbacks */

    if(config->num_buckets == 0 ||
            config->num_workers == 0) return -1; /* Invlaid workers or bucketsquantitiy*/

    if(config->window_size_ms <= 0 || 
            config->slide_interval_ms <= 0) return -1; /*Invalid window configs*/
    if(config->port == 0) return -1; /* Empty port*/
    return 0;
}

trafilo_t *trafilo_create(const trafilo_config_t *config) {
    if(validate_config(config) != 0) return NULL;

    trafilo_t *trafilo = calloc(1, sizeof(trafilo_t));
    if(trafilo == NULL) return NULL;

    trafilo->config = *config;

    // Copy binding addr from config file and manage pointers
    if(config->bind_addr != NULL) {
        trafilo->bind_addr_config = strdup(config->bind_addr);
        if(trafilo->bind_addr_config == NULL) goto fail_addr;//addr fail condition free(trafilo)
        trafilo->config.bind_addr = trafilo->bind_addr_config; // Make sure pointers are right
    }

    // Lifecycle thread protection
    if(pthread_mutex_init(&trafilo->shutdown_lock, NULL) != 0) goto fail_mutex; // mutex fail, Free bind_addr
    if(pthread_cond_init(&trafilo->shutdown_cond, NULL) != 0) goto fail_cond; // cond fail, destroy mutex

    // Build every single module
    trafilo->bounded_q = bq_create(QUEUE_CAPACITY);
    if(trafilo->bounded_q == NULL) goto fail_queue; // Failed queue creation destroy cond variable
    
    trafilo->hash_m = hashmap_create(config->num_buckets);
    if(trafilo->hash_m == NULL) goto fail_hashmap; // Hashmap creation failed free queue

    trafilo->dispatcher = dispatcher_create(trafilo->bounded_q, trafilo->hash_m, trafilo->config);
    if(trafilo->dispatcher == NULL) goto fail_dispatcher; //Dispatcher creation failed free hashmap
                                          
    trafilo->listener = listener_create(config->port, trafilo->bounded_q, MAX_LINE);
    if(trafilo->listener == NULL) goto fail_listener; // Listener creation failed free dispatcher


    return trafilo;
    //Fail conditions
fail_listener : free(trafilo->dispatcher);
fail_dispatcher : free(trafilo->hash_m);
fail_hashmap : free(trafilo->bounded_q);
fail_queue : pthread_cond_destroy(&trafilo->shutdown_cond);
fail_cond : pthread_mutex_destroy(&trafilo->shutdown_lock);
fail_mutex : free(trafilo->bind_addr_config);
fail_addr : free(trafilo);
            return NULL;
}

int trafilo_run(trafilo_t *trafilo) {
    if(trafilo == NULL) return -1;

    // Starting consumers and workers
    if(dispatcher_start(trafilo->dispatcher) != 0) return -1;
    if(listener_start(trafilo->listener) != 0) {
        dispatcher_stop(trafilo->dispatcher);
        return -1;
    } 

    trafilo->running = 1; // Mark start of running

    // Block everything until trafilo_shutdown signals
    // that work is done
    pthread_mutex_lock(&trafilo->shutdown_lock);
    while(!trafilo->shutdown_flag) {
        pthread_cond_wait(&trafilo->shutdown_cond, &trafilo->shutdown_lock);
    }
    pthread_mutex_unlock(&trafilo->shutdown_lock);

    // Stop worker/producers
    listener_stop(trafilo->listener);
    dispatcher_stop(trafilo->dispatcher);

    trafilo->running = 0;
    return 0;
}
int trafilo_emit(trafilo_t *t, const char *raw, size_t len);
void trafilo_destroy(trafilo_t *t);
void trafilo_shutdown(trafilo_t *t);
int main() {return 0;}
