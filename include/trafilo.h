#ifndef TRAFILO
#define TRAFILO

#include <stddef.h>
#include <stdint.h>
#include <time.h>

/**
 * @brief: a struct to hold event data
 */
typedef struct {
    char *key; /* dispatch key (by dispatching service)*/
    void *payload;/* binary safe content of event*/ 
    size_t payload_len; /* binary safe content of event*/
    struct timespec t_secs; /* timestamp of event in nano second precision*/
} event_t;


/*************************
 * User callback functions
 *************************
*/

/**
 * @brief: function to parse and fillout **out with a fresh event_t
 * @param char *raw: raw log entry from stream
 * @param size_t len: length of the raw entry
 * @param event_t **out: new parsed entry along with event data
 * @return 0 on success, -1 to drop entry
 */
typedef int (*trafilo_parse_fn)(const char *raw, size_t len, event_t **out);

/**
 * @brief: Free a user allocated event from memory
 * @param event_t *event: Event to be freed.
 */
typedef void (*trafilo_event_free_fn)(event_t *event);


/**
 * @brief: Handling callback function provided by user
 * @param event_t *event: Event to be handled.
 * @param void *user_state: Bucket's state pointer.
 */
typedef void (*trafilo_handle_fn)(const event_t *event, void *user_state);

/**
 * @brief: Sink window 
 * @param char *key: Bucket identifier (service identifier).
 * @param void *user_state: Bucket's state pointer.
 */
typedef void (*trafilo_sink_fn)(const char *key,
                                const window_result_t *result,
                                void *user_state);

/**
 * @brief: Initalize the state of a bucket, called the first time a key is seen 
 * @param char *key: Bucket specific key.
 * @param void *user_state: Bucket's state pointer.
 */
typedef void *(*trafilo_state_init_fn)(const char *key);

/**
 * @brief: Free the user defined state, when a bucket is evicted. 
 * @param void *user_state: State to be freed.
 */
typedef void (*trafilo_state_free_fn)(void *user_state);

typedef struct {
    /* Network */
    const char *bind_addr;              /* Stream output address*/
    uint16_t    port;                   /* UDP port to bind to */
    int         recv_timeout_ms;        /* SO_RCVTIMEO; 0 = block forever*/

    /* Threading and Concurency*/
    size_t num_workers;                 /* worker thread count */
    size_t num_buckets;                 /* hashmap size, prime number*/

    /* Sliding Window */
    long window_size_ms;                /* sliding window length */
    long slide_interval_ms;             /* slide interval, which also determines emit cadence */
    long bucket_idle_timeout_ms;        /* evict bucket if no events for this
                                           long; 0 = never evict */

    /* User callback functions */
    trafilo_parse_fn       parse;
    trafilo_event_free_fn  event_free;
    trafilo_handle_fn      handle;
    trafilo_sink_fn        sink;
    trafilo_state_init_fn  state_init;
    trafilo_state_free_fn  state_free;
} trafilo_config_t;

#endif
