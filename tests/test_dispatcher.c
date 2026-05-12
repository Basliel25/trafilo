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
//Runner
int main(void) {
    UNITY_BEGIN();

    //RUN_TEST(test_create_returns_non_null_with_valid_args);
    //RUN_TEST(test_create_rejects_null_queue);
    //RUN_TEST(test_create_rejects_null_hashmap);
    //RUN_TEST(test_create_rejects_null_config);
    //RUN_TEST(test_create_rejects_zero_workers);
    //RUN_TEST(test_start_then_stop_joins_cleanly);
    //RUN_TEST(test_double_start_returns_already_started);
    //RUN_TEST(test_stop_is_idempotent);
    //RUN_TEST(test_destroy_without_start_does_not_leak);
    //RUN_TEST(test_destroy_chains_through_stop);

    return UNITY_END();
}
