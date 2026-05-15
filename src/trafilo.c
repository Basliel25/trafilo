#include "headers/bounded_queue.h"
#include "headers/hashmap.h"
#include "headers/window.h"
#include "headers/socket.h"
#include "headers/dispatcher.h"
#include "../include/trafilo.h"

#define QUEUE_CAPACITY 4096
#define MAX_LINE 2048

struct trafilo {
    trafilo_config_t conifg; /*< Owned copy of trafilo config*/
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

trafilo_t *trafilo_create(const trafilo_config_t *cfg);

int main() {return 0;}
