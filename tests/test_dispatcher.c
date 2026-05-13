#include "unity.h"
#include "../src/headers/bounded_queue.h"
#include "../src/headers/hashmap.h"
#include "../src/headers/dispatcher.h"
#include "../include/trafilo.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>  

static int dummy_parse(const char *raw, size_t len, event_t **out) {
    (void)raw;
    (void)len;
    (void)out;
    return -1;  
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
    dispatcher_stop(d);  

    dispatcher_destroy(d);
}

void test_destroy_without_start_does_not_leak(void) {
    dispatcher_t *d = dispatcher_create(bq, hm, &cfg);
    TEST_ASSERT_NOT_NULL(d);
    dispatcher_destroy(d);
}

void test_destroy_chains_through_stop(void) {
    dispatcher_t *d = dispatcher_create(bq, hm, &cfg);
    TEST_ASSERT_NOT_NULL(d);

    dispatcher_start(d);
    dispatcher_destroy(d);  /* must call stop internally — no hang */
    TEST_ASSERT_TRUE(1);
}

// Callbacks

static int parse_calls;
static int handle_calls;
static int event_free_calls;
static int state_init_calls;
static int state_free_calls;
static int parse_should_fail; 

typedef struct {
    int    called_count;
    char   last_key[64];
    size_t last_event_count;
} sink_capture_t;

static sink_capture_t sink_capture;

typedef struct {
    int seen_events;
    char key_copy[64];  
} test_state_t;

static int counting_parse(const char *raw, size_t len, event_t **out) {
    parse_calls++;
    if (parse_should_fail) return -1;

    event_t *e = malloc(sizeof(event_t));
    if (e == NULL) return -1;

    e->key = strdup("svc");  /* same key for every line → one bucket */
    if (e->key == NULL) { free(e); return -1; }

    e->payload     = NULL;
    e->payload_len = 0;
    clock_gettime(CLOCK_MONOTONIC, &e->t_secs);

    (void)raw;
    (void)len;
    *out = e;
    return 0;
}

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

static void counting_sink(const char *key,
                          const window_result_t *result,
                          void *user_state) {
    sink_capture.called_count++;
    if (key) {
        strncpy(sink_capture.last_key, key, sizeof(sink_capture.last_key) - 1);
        sink_capture.last_key[sizeof(sink_capture.last_key) - 1] = '\0';
    }
    if (result) {
        sink_capture.last_event_count = result->event_count;
    }
    (void)user_state;
}

// Helpers

static void reset_counters(void) {
    parse_calls       = 0;
    handle_calls      = 0;
    event_free_calls  = 0;
    state_init_calls  = 0;
    state_free_calls  = 0;
    parse_should_fail = 0;
    memset(&sink_capture, 0, sizeof(sink_capture));
}

static void install_counting_callbacks(void) {
    cfg.parse       = counting_parse;
    cfg.handle      = counting_handle;
    cfg.event_free  = counting_event_free;
    cfg.state_init  = counting_state_init;
    cfg.state_free  = counting_state_free;
    cfg.num_workers = 1;  

    cfg.window_size_ms    = 60000;  /* 1 minute */
    cfg.slide_interval_ms = 60000;  
    cfg.sink              = NULL;   /* off by default */
}

static void push_line(const char *s) {
    char *copy = strdup(s);
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQUAL_INT(0, bq_push(bq, copy));
}

// Tests

void test_single_line_drives_parse_handle_free_once(void) {
    reset_counters();
    install_counting_callbacks();

    push_line("hello");

    dispatcher_t *d = dispatcher_create(bq, hm, &cfg);
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_EQUAL_INT(0, dispatcher_start(d));
    dispatcher_stop(d);

    TEST_ASSERT_EQUAL_INT(1, parse_calls);
    TEST_ASSERT_EQUAL_INT(1, handle_calls);
    TEST_ASSERT_EQUAL_INT(1, event_free_calls);

    dispatcher_destroy(d);
}

void test_state_init_called_exactly_once_per_key(void) {
    reset_counters();
    install_counting_callbacks();

    for (int i = 0; i < 5; i++) push_line("anything");

    dispatcher_t *d = dispatcher_create(bq, hm, &cfg);
    dispatcher_start(d);
    dispatcher_stop(d);

    TEST_ASSERT_EQUAL_INT(5, parse_calls);
    TEST_ASSERT_EQUAL_INT(5, handle_calls);
    TEST_ASSERT_EQUAL_INT(5, event_free_calls);
    TEST_ASSERT_EQUAL_INT(1, state_init_calls);  

    dispatcher_destroy(d);
}

void test_state_accumulates_across_lines(void) {
    reset_counters();
    install_counting_callbacks();

    push_line("a");
    push_line("b");
    push_line("c");

    dispatcher_t *d = dispatcher_create(bq, hm, &cfg);
    dispatcher_start(d);
    dispatcher_stop(d);

    bucket_node *bucket = hashmap_find_or_create(hm, "svc");
    TEST_ASSERT_NOT_NULL(bucket);
    test_state_t *s = (test_state_t *)bucket->state;
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(3, s->seen_events);
    TEST_ASSERT_EQUAL_STRING("svc", s->key_copy);
    hashmap_unlock_bucket(hm, "svc");

    dispatcher_destroy(d);
}

void test_parse_failure_drops_line_no_handle(void) {
    reset_counters();
    install_counting_callbacks();
    parse_should_fail = 1;

    push_line("garbage1");
    push_line("garbage2");

    dispatcher_t *d = dispatcher_create(bq, hm, &cfg);
    dispatcher_start(d);
    dispatcher_stop(d);

    TEST_ASSERT_EQUAL_INT(2, parse_calls);
    TEST_ASSERT_EQUAL_INT(0, handle_calls);     
    TEST_ASSERT_EQUAL_INT(0, event_free_calls); 
    TEST_ASSERT_EQUAL_INT(0, state_init_calls); 

    dispatcher_destroy(d);
}

void test_distinct_keys_get_distinct_buckets(void) {
    reset_counters();
    install_counting_callbacks();
    cfg.parse = per_line_key_parse;

    push_line("alpha");
    push_line("beta");
    push_line("gamma");

    dispatcher_t *d = dispatcher_create(bq, hm, &cfg);
    dispatcher_start(d);
    dispatcher_stop(d);

    TEST_ASSERT_EQUAL_INT(3, parse_calls);
    TEST_ASSERT_EQUAL_INT(3, handle_calls);
    TEST_ASSERT_EQUAL_INT(3, event_free_calls);
    TEST_ASSERT_EQUAL_INT(3, state_init_calls);  

    dispatcher_destroy(d);
}

void test_repeated_keys_share_state(void) {
    reset_counters();
    install_counting_callbacks();
    cfg.parse = per_line_key_parse;

    push_line("alpha");
    push_line("beta");
    push_line("alpha");
    push_line("beta");

    dispatcher_t *d = dispatcher_create(bq, hm, &cfg);
    dispatcher_start(d);
    dispatcher_stop(d);

    TEST_ASSERT_EQUAL_INT(4, handle_calls);
    TEST_ASSERT_EQUAL_INT(2, state_init_calls);

    bucket_node *ba = hashmap_find_or_create(hm, "alpha");
    test_state_t *sa = (test_state_t *)ba->state;
    TEST_ASSERT_EQUAL_INT(2, sa->seen_events);
    hashmap_unlock_bucket(hm, "alpha");

    bucket_node *bb = hashmap_find_or_create(hm, "beta");
    test_state_t *sb = (test_state_t *)bb->state;
    TEST_ASSERT_EQUAL_INT(2, sb->seen_events);
    hashmap_unlock_bucket(hm, "beta");

    dispatcher_destroy(d);
}

// sink tests

void test_sink_fires_on_first_event(void) {
    reset_counters();
    install_counting_callbacks();
    cfg.sink              = counting_sink;
    cfg.window_size_ms    = 1000;
    cfg.slide_interval_ms = 500;

    push_line("first");

    dispatcher_t *d = dispatcher_create(bq, hm, &cfg);
    TEST_ASSERT_NOT_NULL(d);
    dispatcher_start(d);
    dispatcher_stop(d);

    TEST_ASSERT_EQUAL_INT(1, sink_capture.called_count);
    TEST_ASSERT_EQUAL_STRING("svc", sink_capture.last_key);
    TEST_ASSERT_EQUAL_size_t(1, sink_capture.last_event_count);

    dispatcher_destroy(d);
}

void test_sink_does_not_fire_before_cadence_elapses(void) {
    reset_counters();
    install_counting_callbacks();
    cfg.sink              = counting_sink;
    cfg.window_size_ms    = 60000;
    cfg.slide_interval_ms = 10000;

    push_line("a");
    push_line("b");
    push_line("c");

    dispatcher_t *d = dispatcher_create(bq, hm, &cfg);
    dispatcher_start(d);
    dispatcher_stop(d);

    TEST_ASSERT_EQUAL_INT(3, handle_calls);          
    TEST_ASSERT_EQUAL_INT(1, sink_capture.called_count); 

    dispatcher_destroy(d);
}

void test_sink_fires_again_after_cadence_elapses(void) {
    reset_counters();
    install_counting_callbacks();
    cfg.sink              = counting_sink;
    cfg.window_size_ms    = 1000;
    cfg.slide_interval_ms = 50;   /* 50ms cadence */

    push_line("alpha");

    dispatcher_t *d = dispatcher_create(bq, hm, &cfg);
    dispatcher_start(d);

    usleep(20000);   /* 20ms */

    usleep(100000);  /* 100ms — 2x cadence */

    push_line("beta");

    dispatcher_stop(d);

    TEST_ASSERT_EQUAL_INT(2, handle_calls);
    TEST_ASSERT_EQUAL_INT(2, sink_capture.called_count);

    dispatcher_destroy(d);
}

// Runner
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
    RUN_TEST(test_single_line_drives_parse_handle_free_once);
    RUN_TEST(test_state_init_called_exactly_once_per_key);
    RUN_TEST(test_state_accumulates_across_lines);
    RUN_TEST(test_parse_failure_drops_line_no_handle);
    RUN_TEST(test_distinct_keys_get_distinct_buckets);
    RUN_TEST(test_repeated_keys_share_state);
    RUN_TEST(test_sink_fires_on_first_event);
    RUN_TEST(test_sink_does_not_fire_before_cadence_elapses);
    RUN_TEST(test_sink_fires_again_after_cadence_elapses);

    return UNITY_END();
}
