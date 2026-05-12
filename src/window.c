#include "headers/window.h"


static int64_t timespec_diff(struct timespec first, struct timespec second);

void sliding_window_init(sliding_window_t *w,
                         long window_size_ms,
                         long slide_ms) {
    if(w == NULL) return;
    w->window_size_ms = window_size_ms;
    w->slide_ms = slide_ms;

    w->count = 0;
    
    ts_node_t  *head = NULL, *tail = NULL;
    head->next = tail;
    w->head = head;
    w->tail = tail;

    w->last_emit = (struct timespec){-1};
}

void sliding_window_destroy(sliding_window_t *w) {
    ts_node_t *current = w->head;
    while(current != NULL) {
        ts_node_t *next = current->next;
        free(current);
        current = next;
    }
    w->head = NULL;
    w->tail = NULL;
    w->count = 0;
    
}
int  sliding_window_add(sliding_window_t *w, struct timespec event_ts) {
    if(w == NULL) return -1;
    
    ts_node_t *new_node = malloc(sizeof(ts_node_t));
    if(new_node == NULL) return -1;

    new_node->ts = event_ts;
    new_node->next = NULL;

    if(w->tail != NULL) {
        w->tail->next = new_node;
        w->tail = new_node;
    } else {
        w->head = new_node;
        w->tail = new_node;
    }

    w->count += 1;
    return 0;
}
// Handle last_emit being -1 as sentinel
int  sliding_window_should_emit(const sliding_window_t *w, struct timespec event_ts);

void sliding_window_mark_emitted(sliding_window_t *w, struct timespec now);
size_t          sliding_window_count(const sliding_window_t *w) {return w->count;}
struct timespec sliding_window_oldest(const sliding_window_t *w);
struct timespec sliding_window_newest(const sliding_window_t *w);
