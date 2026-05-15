#include "headers/bounded_queue.h"
#include "headers/hashmap.h"
#include "headers/window.h"
#include "headers/socket.h"
#include "headers/dispatcher.h"
#include "../include/trafilo.h"

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

int main() {return 0;}
