#include "unity.h"
#include "../src/headers/bounded_queue.h"
#include "../src/headers/hashmap.h"
#include "../src/headers/dispatcher.h"
#include "../include/trafilo.h"

#include <stdlib.h>
#include <string.h>


// Dummy callbacks since dispatcher is currently an empty loop
static event_t *dummy_parse(const char *line) {
    (void)line;
    return NULL;
}

static void dummy_handle(bucket_node *bucket, const event_t *event) {
    (void)bucket;
    (void)event;
}

static void dummy_event_free(event_t *event) {
    (void)event;
}


// Fixtures

// Tests

//Runner
int main(void) {
    UNITY_BEGIN();

    return UNITY_END();
}
