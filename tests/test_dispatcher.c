#include "unity.h"
#include "../src/headers/bounded_queue.h"
#include "../src/headers/hashmap.h"
#include "../src/headers/dispatcher.h"
#include "../include/trafilo.h"

#include <stdlib.h>
#include <string.h>


// Dummy callbacks since dispatcher is currently an empty loop
static int dummy_parse(const char *raw, size_t len, event_t **out) {
    (void)raw;
    (void)len;
    (void)out;
    return -1;  /* always drop — loop is a stub, this never runs */
}
 
static void dummy_handle(const event_t *event, void *user_state) {
    (void)event;
    (void)user_state;
}
 
static void dummy_event_free(event_t *event) {
    (void)event;
}

// Fixtures

static bounded_queue_t *bq;
static hashmap_t       *hm;
static trafilo_config_t cfg;

void setUp(void) {
    bq = bq_create(64);
    hm = hashmap_create(16);

    memset(&cfg, 0, sizeof(cfg));
    cfg.parse       = dummy_parse;
    cfg.handle      = dummy_handle;
    cfg.event_free  = dummy_event_free;
    cfg.num_workers = 4;
}

void tearDown(void) {
    bq_destroy(bq);
    hashmap_destroy(hm, NULL);
}
// Tests

void test_create_returns_non_null_with_valid_args(void) {
    dispatcher_t *d = dispatcher_create(bq, hm, &cfg);
    TEST_ASSERT_NOT_NULL(d);
    dispatcher_destroy(d);
}

void test_create_rejects_null_queue(void) {
    dispatcher_t *d = dispatcher_create(NULL, hm, &cfg);
    TEST_ASSERT_NULL(d);
}

void test_create_rejects_null_hashmap(void) {
    dispatcher_t *d = dispatcher_create(bq, NULL, &cfg);
    TEST_ASSERT_NULL(d);
}

void test_create_rejects_null_config(void) {
    dispatcher_t *d = dispatcher_create(bq, hm, NULL);
    TEST_ASSERT_NULL(d);
}

void test_create_rejects_zero_workers(void) {
    cfg.num_workers = 0;
    dispatcher_t *d = dispatcher_create(bq, hm, &cfg);
    TEST_ASSERT_NULL(d);
}

void test_start_then_stop_joins_cleanly(void) {
    dispatcher_t *d = dispatcher_create(bq, hm, &cfg);
    TEST_ASSERT_NOT_NULL(d);

    int rc = dispatcher_start(d);
    TEST_ASSERT_EQUAL_INT(0, rc);

    dispatcher_stop(d);
    /* if we reach here without hanging, join worked */
    TEST_ASSERT_TRUE(1);

    dispatcher_destroy(d);
}

void test_double_start_returns_already_started(void) {
    dispatcher_t *d = dispatcher_create(bq, hm, &cfg);
    TEST_ASSERT_NOT_NULL(d);

    TEST_ASSERT_EQUAL_INT(0, dispatcher_start(d));
    TEST_ASSERT_EQUAL_INT(-2, dispatcher_start(d));

    dispatcher_stop(d);
    dispatcher_destroy(d);
}

void test_stop_is_idempotent(void) {
    dispatcher_t *d = dispatcher_create(bq, hm, &cfg);
    TEST_ASSERT_NOT_NULL(d);

    dispatcher_start(d);
    dispatcher_stop(d);
    dispatcher_stop(d);  /* second call must be a no-op, not a crash */

    dispatcher_destroy(d);
}

void test_destroy_without_start_does_not_leak(void) {
    /* Created but never started — destroy must still free everything.
     * Verify with valgrind; this test only confirms no crash. */
    dispatcher_t *d = dispatcher_create(bq, hm, &cfg);
    TEST_ASSERT_NOT_NULL(d);
    dispatcher_destroy(d);
}

void test_destroy_chains_through_stop(void) {
    /* destroy on a running dispatcher must stop it first */
    dispatcher_t *d = dispatcher_create(bq, hm, &cfg);
    TEST_ASSERT_NOT_NULL(d);

    dispatcher_start(d);
    dispatcher_destroy(d);  /* must call stop internally — no hang */
    TEST_ASSERT_TRUE(1);
}
/****
 * Test worker Loop
 */
 
//Counters and callbacks
 
static int parse_calls;
static int handle_calls;
static int event_free_calls;
static int state_init_calls;
static int state_free_calls;
static int parse_should_fail; /* if 1, parse returns -1 (drop) */
 
typedef struct {
    int seen_events;
    char key_copy[64];  
} test_state_t;
 
static int counting_parse(const char *raw, size_t len, event_t **out) {
    parse_calls++;
    if (parse_should_fail) return -1;
 
    event_t *e = malloc(sizeof(event_t));
    if (e == NULL) return -1;
 
    e->key = strdup("svc");  
    if (e->key == NULL) { free(e); return -1; }
 
    e->payload     = NULL;
    e->payload_len = 0;
    clock_gettime(CLOCK_MONOTONIC, &e->t_secs);
 
    (void)raw;
    (void)len;
    *out = e;
    return 0;
}
 
// parse variant that uses the raw line content as the key 
static int per_line_key_parse(const char *raw, size_t len, event_t **out) {
    parse_calls++;
    event_t *e = malloc(sizeof(event_t));
    if (e == NULL) return -1;
 
    e->key = strdup(raw);
    if (e->key == NULL) { free(e); return -1; }
 
    e->payload     = NULL;
    e->payload_len = 0;
    clock_gettime(CLOCK_MONOTONIC, &e->t_secs);
 
    (void)len;
    *out = e;
    return 0;
}
 
static void counting_handle(const event_t *event, void *user_state) {
    handle_calls++;
    if (user_state) {
        test_state_t *s = (test_state_t *)user_state;
        s->seen_events++;
        if (event && event->key) {
            strncpy(s->key_copy, event->key, sizeof(s->key_copy) - 1);
        }
    }
}
 
static void counting_event_free(event_t *event) {
    event_free_calls++;
    if (event) {
        free(event->key);
        free(event);
    }
}
 
static void *counting_state_init(const char *key) {
    state_init_calls++;
    test_state_t *s = calloc(1, sizeof(test_state_t));
    (void)key;
    return s;
}
 
static void counting_state_free(void *user_state) {
    state_free_calls++;
    free(user_state);
}
 
static void reset_counters(void) {
    parse_calls       = 0;
    handle_calls      = 0;
    event_free_calls  = 0;
    state_init_calls  = 0;
    state_free_calls  = 0;
    parse_should_fail = 0;
}
 
static void install_counting_callbacks(void) {
    cfg.parse       = counting_parse;
    cfg.handle      = counting_handle;
    cfg.event_free  = counting_event_free;
    cfg.state_init  = counting_state_init;
    cfg.state_free  = counting_state_free;
    cfg.num_workers = 1;  /* serialize → counter math is exact */
}
 
static void push_line(const char *s) {
    /* worker will free() this — must be heap-allocated */
    char *copy = strdup(s);
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQUAL_INT(0, bq_push(bq, copy));
}
 
/* ---- helpers ------------------------------------------------------- */

static void reset_counters(void) {
    parse_calls       = 0;
    handle_calls      = 0;
    event_free_calls  = 0;
    state_init_calls  = 0;
    state_free_calls  = 0;
    parse_should_fail = 0;
}
 
static void install_counting_callbacks(void) {
    cfg.parse       = counting_parse;
    cfg.handle      = counting_handle;
    cfg.event_free  = counting_event_free;
    cfg.state_init  = counting_state_init;
    cfg.state_free  = counting_state_free;
    cfg.num_workers = 1;  /* serialize → counter math is exact */
}
 
static void push_line(const char *s) {
    /* worker will free() this — must be heap-allocated */
    char *copy = strdup(s);
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQUAL_INT(0, bq_push(bq, copy));
}
/* ---- tests ------------------------------------------------------- */
//Runner
int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_create_returns_non_null_with_valid_args);
    RUN_TEST(test_create_rejects_null_queue);
    RUN_TEST(test_create_rejects_null_hashmap);
    RUN_TEST(test_create_rejects_null_config);
    RUN_TEST(test_create_rejects_zero_workers);
    RUN_TEST(test_start_then_stop_joins_cleanly);
    RUN_TEST(test_double_start_returns_already_started);
    RUN_TEST(test_stop_is_idempotent);
    RUN_TEST(test_destroy_without_start_does_not_leak);
    RUN_TEST(test_destroy_chains_through_stop);

    return UNITY_END();
}
