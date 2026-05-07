#include "unity.h"
#include "../src/headers/hashmap.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── helper: simple state_free for tests that put a malloc'd int as state ── */

static void free_int_state(void *state) {
    free(state);
}

static void noop_state_free(void *state) {
    (void)state;
}

/* ── lifecycle ────────────────────────────────────────── */

void test_create_destroy_empty(void) {
    hashmap_t *m = hashmap_create(16);
    TEST_ASSERT_NOT_NULL(m);
    hashmap_destroy(m, noop_state_free);
}

void test_create_zero_buckets_returns_null(void) {
    hashmap_t *m = hashmap_create(0);
    TEST_ASSERT_NULL(m);
}

/* ── find_or_create: basic ────────────────────────────── */

void test_find_or_create_inserts_new_key(void) {
    hashmap_t *m = hashmap_create(16);

    bucket_node *b = hashmap_find_or_create(m, "sshd");
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQUAL_STRING("sshd", b->key);
    hashmap_unlock_bucket(m, "sshd");

    hashmap_destroy(m, noop_state_free);
}

void test_find_or_create_returns_same_bucket_for_same_key(void) {
    hashmap_t *m = hashmap_create(16);

    bucket_node *b1 = hashmap_find_or_create(m, "kernel");
    hashmap_unlock_bucket(m, "kernel");

    bucket_node *b2 = hashmap_find_or_create(m, "kernel");
    hashmap_unlock_bucket(m, "kernel");

    TEST_ASSERT_EQUAL_PTR(b1, b2);

    hashmap_destroy(m, noop_state_free);
}

void test_find_or_create_distinct_keys_distinct_buckets(void) {
    hashmap_t *m = hashmap_create(16);

    bucket_node *a = hashmap_find_or_create(m, "sshd");
    hashmap_unlock_bucket(m, "sshd");
    bucket_node *b = hashmap_find_or_create(m, "kernel");
    hashmap_unlock_bucket(m, "kernel");

    TEST_ASSERT_NOT_EQUAL(a, b);
    TEST_ASSERT_EQUAL_STRING("sshd",   a->key);
    TEST_ASSERT_EQUAL_STRING("kernel", b->key);

    hashmap_destroy(m, noop_state_free);
}

/* ── chain walk: force collisions with num_buckets=1 ──── */

void test_chain_walk_with_forced_collisions(void) {
    /* one bucket — every key collides, exercises chain insert + chain find */
    hashmap_t *m = hashmap_create(1);

    bucket_node *a = hashmap_find_or_create(m, "alpha");
    hashmap_unlock_bucket(m, "alpha");
    bucket_node *b = hashmap_find_or_create(m, "beta");
    hashmap_unlock_bucket(m, "beta");
    bucket_node *c = hashmap_find_or_create(m, "gamma");
    hashmap_unlock_bucket(m, "gamma");

    /* re-find each — must return the original node, not a new one */
    bucket_node *a2 = hashmap_find_or_create(m, "alpha");
    hashmap_unlock_bucket(m, "alpha");
    bucket_node *b2 = hashmap_find_or_create(m, "beta");
    hashmap_unlock_bucket(m, "beta");
    bucket_node *c2 = hashmap_find_or_create(m, "gamma");
    hashmap_unlock_bucket(m, "gamma");

    TEST_ASSERT_EQUAL_PTR(a, a2);
    TEST_ASSERT_EQUAL_PTR(b, b2);
    TEST_ASSERT_EQUAL_PTR(c, c2);

    hashmap_destroy(m, noop_state_free);
}

/* ── state ownership: state_free called on destroy ────── */

static int free_count = 0;
static void counting_state_free(void *state) {
    free(state);
    free_count++;
}

void test_state_free_called_for_each_bucket(void) {
    hashmap_t *m = hashmap_create(8);
    free_count = 0;

    const char *keys[] = { "a", "b", "c", "d", "e" };
    for (int i = 0; i < 5; i++) {
        bucket_node *b = hashmap_find_or_create(m, keys[i]);
        b->state = malloc(sizeof(int));
        *(int *)b->state = i;
        hashmap_unlock_bucket(m, keys[i]);
    }

    hashmap_destroy(m, counting_state_free);
    TEST_ASSERT_EQUAL_INT(5, free_count);
}

/* ── for_each iterates every bucket ───────────────────── */

static void count_visit(bucket_node *bucket, void *arg) {
    (void)bucket;
    int *counter = arg;
    (*counter)++;
}

void test_for_each_visits_every_bucket(void) {
    hashmap_t *m = hashmap_create(16);

    const char *keys[] = { "a", "b", "c", "d", "e", "f", "g" };
    for (int i = 0; i < 7; i++) {
        bucket_node *b = hashmap_find_or_create(m, keys[i]);
        hashmap_unlock_bucket(m, keys[i]);
    }

    int visits = 0;
    hashmap_for_each(m, count_visit, &visits);
    TEST_ASSERT_EQUAL_INT(7, visits);

    hashmap_destroy(m, noop_state_free);
}

/* ── threading: hammer find_or_create from N threads ──── */

#define N_THREADS    8
#define N_OPS        2000
#define KEY_SPACE    32   /* small key space → lots of collisions/contention */

typedef struct {
    hashmap_t *m;
} hammer_args_t;

static void *hammer(void *arg) {
    hammer_args_t *a = arg;
    char keybuf[16];
    for (int i = 0; i < N_OPS; i++) {
        snprintf(keybuf, sizeof keybuf, "k%d", i % KEY_SPACE);
        bucket_node *b = hashmap_find_or_create(a->m, keybuf);
        if (b == NULL) continue;

        /* read-modify-write under the bucket lock */
        if (b->state == NULL) {
            b->state = calloc(1, sizeof(int));
        }
        if (b->state) {
            (*(int *)b->state)++;
        }
        hashmap_unlock_bucket(a->m, keybuf);
    }
    return NULL;
}

static void sum_visit(bucket_node *bucket, void *arg) {
    int *total = arg;
    if (bucket->state) {
        *total += *(int *)bucket->state;
    }
}

void test_threaded_find_or_create_no_lost_updates(void) {
    /* num_buckets smaller than key space → chain walks under contention */
    hashmap_t *m = hashmap_create(8);

    pthread_t threads[N_THREADS];
    hammer_args_t args = { .m = m };

    for (int i = 0; i < N_THREADS; i++) {
        pthread_create(&threads[i], NULL, hammer, &args);
    }
    for (int i = 0; i < N_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    /* every op incremented exactly one counter — total must be exact */
    int total = 0;
    hashmap_for_each(m, sum_visit, &total);
    TEST_ASSERT_EQUAL_INT(N_THREADS * N_OPS, total);

    hashmap_destroy(m, free_int_state);
}

int main(void) {
    UNITY_BEGIN();

    //RUN_TEST(test_create_destroy_empty);
    //RUN_TEST(test_create_zero_buckets_returns_null);

    //RUN_TEST(test_find_or_create_inserts_new_key);
    //RUN_TEST(test_find_or_create_returns_same_bucket_for_same_key);
    //RUN_TEST(test_find_or_create_distinct_keys_distinct_buckets);

    //RUN_TEST(test_chain_walk_with_forced_collisions);

    //RUN_TEST(test_state_free_called_for_each_bucket);
    //RUN_TEST(test_for_each_visits_every_bucket);

    //RUN_TEST(test_threaded_find_or_create_no_lost_updates);

    return UNITY_END();
}
