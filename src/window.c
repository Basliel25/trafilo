#include "headers/window.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Helper to calculate difference in milliseconds between two timespecs.
 *        Returns long (end - start).
 */
static long timespec_diff_ms(struct timespec start, struct timespec end) {

    // Use long long because starting from epoch to now in seconds is greater that 64bit
    long long start_ms = (long long)start.tv_sec * 1000 + (start.tv_nsec / 1000000);
    long long end_ms = (long long)end.tv_sec * 1000 + (end.tv_nsec / 1000000);
    return (long)(end_ms - start_ms);
}

void sliding_window_init(sliding_window_t *w, long window_size_ms, long slide_ms) {
    if (!w) return;
    w->head = NULL;
    w->tail = NULL;
    w->count = 0;
    w->window_size_ms = window_size_ms;
    w->slide_ms = slide_ms;
    w->last_emit.tv_sec = 0;
    w->last_emit.tv_nsec = 0;
}

void sliding_window_destroy(sliding_window_t *w) {
    if (!w) return;
    ts_node_t *curr = w->head;
    while (curr) {
        ts_node_t *next = curr->next;
        free(curr);
        curr = next;
    }
    w->head = NULL;
    w->tail = NULL;
    w->count = 0;
}

int sliding_window_add(sliding_window_t *w, struct timespec event_ts) {
    if (w == NULL) return -1;

    ts_node_t *new_node = malloc(sizeof(ts_node_t));
    if (new_node == NULL) return -1;

    new_node->ts = event_ts;
    new_node->next = NULL;

    // Append to tail
    if (w->tail) {
        w->tail->next = new_node;
        w->tail = new_node;
    } else {
        w->head = new_node;
        w->tail = new_node;
    }
    w->count++;

    // Prune events that fell out of the window span
    while (w->head && timespec_diff_ms(w->head->ts, event_ts) > w->window_size_ms) {
        ts_node_t *old = w->head;
        w->head = w->head->next;
        
        if (w->head == NULL) {
            w->tail = NULL;
        }
        
        free(old);
        w->count--;
    }

    return 0;
}

