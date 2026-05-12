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

//Runner
int main(void) {
    UNITY_BEGIN();

    return UNITY_END();
}
