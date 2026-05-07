#include "unity.h"
#include "../src/headers/bounded_queue.h"

void setUp(void) {}
void tearDown(void) {}

/* ── basic push/pop ───────────────────────────────────── */

void test_push_pop_single_item(void) {
    bounded_queue_t *q = bq_create(8);
    TEST_ASSERT_NOT_NULL(q);

    char *item = "hello";
    int rc = bq_push(q, item);
    TEST_ASSERT_EQUAL_INT(0, rc);

    char *out = bq_pop(q);
    TEST_ASSERT_EQUAL_STRING("hello", out);

    bq_destroy(q);
}

void test_fifo_ordering(void) {
    bounded_queue_t *q = bq_create(8);

    bq_push(q, "first");
    bq_push(q, "second");
    bq_push(q, "third");

    TEST_ASSERT_EQUAL_STRING("first",  bq_pop(q));
    TEST_ASSERT_EQUAL_STRING("second", bq_pop(q));
    TEST_ASSERT_EQUAL_STRING("third",  bq_pop(q));

    bq_destroy(q);
}

void test_capacity_zero_returns_null(void) {
    bounded_queue_t *q = bq_create(0);
    TEST_ASSERT_NULL(q);
}

void test_pop_after_shutdown_returns_null(void) {
    bounded_queue_t *q = bq_create(8);

    bq_shutdown(q);
    void *out = bq_pop(q);
    TEST_ASSERT_NULL(out);

    bq_destroy(q);
}

void test_push_after_shutdown_returns_error(void) {
    bounded_queue_t *q = bq_create(8);

    bq_shutdown(q);
    int rc = bq_push(q, "item");
    TEST_ASSERT_EQUAL_INT(-1, rc);

    bq_destroy(q);
}

/* Threading */

#include <pthread.h>

#define ITEM_COUNT 64

typedef struct {
    bounded_queue_t *q;
    int              count;
} thread_args_t;

static void *producer(void *arg) {
    thread_args_t *a = arg;
    for (int i = 0; i < a->count; i++)
        bq_push(a->q, "item");
    return NULL;
}

static void *consumer(void *arg) {
    thread_args_t *a = arg;
    int received = 0;
    while (bq_pop(a->q) != NULL)
        received++;
    *(int *)arg = received;   
    return NULL;
}

void test_threaded_producer_consumer(void) {
    bounded_queue_t *q = bq_create(16);

    thread_args_t pargs = { .q = q, .count = ITEM_COUNT };
    thread_args_t cargs = { .q = q, .count = 0 };

    pthread_t prod, cons;
    pthread_create(&cons, NULL, consumer, &cargs);
    pthread_create(&prod, NULL, producer, &pargs);

    pthread_join(prod, NULL);
    bq_shutdown(q);           /* wake consumer after producer done */
    pthread_join(cons, NULL);

    /* consumer counted how many items it received */
    TEST_ASSERT_EQUAL_INT(ITEM_COUNT, cargs.count);

    bq_destroy(q);
}


void test_bounded_capacity_respected(void) {
    bounded_queue_t *q = bq_create(4);

    /* fill to capacity */
    TEST_ASSERT_EQUAL_INT(0, bq_push(q, "a"));
    TEST_ASSERT_EQUAL_INT(0, bq_push(q, "b"));
    TEST_ASSERT_EQUAL_INT(0, bq_push(q, "c"));
    TEST_ASSERT_EQUAL_INT(0, bq_push(q, "d"));

    bq_pop(q);
    TEST_ASSERT_EQUAL_INT(0, bq_push(q, "e"));

    bq_shutdown(q);
    bq_destroy(q);
}


int main(void) {
    UNITY_BEGIN();

    //RUN_TEST(test_push_pop_single_item);
    //RUN_TEST(test_fifo_ordering);
    //RUN_TEST(test_capacity_zero_returns_null);
    //RUN_TEST(test_pop_after_shutdown_returns_null);
    //RUN_TEST(test_push_after_shutdown_returns_error);
    //RUN_TEST(test_threaded_producer_consumer);
    //RUN_TEST(test_bounded_capacity_respected);

    return UNITY_END();
}
