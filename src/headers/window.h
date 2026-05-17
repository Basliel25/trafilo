#ifndef WINDOW_H
#define WINDOW_H

#include <time.h>
#include <stdlib.h>
#include <stdint.h>

/**
 * @brief A timestamp node.
 */
typedef struct ts_node {
    struct timespec  ts;
    struct ts_node  *next;
} ts_node_t;

/**
 * @brief A sliding sampling window represented as a linked list of timestamps.
 */
typedef struct sliding_window_t {
    ts_node_t   *head;             /* oldest retained timestamp, eviction happens here */
    ts_node_t   *tail;             /* newest retained timestamp, insertion happens here*/
    size_t       count;            /* deque size */

    long         window_size_ms;   /* retention horizon */
    long         slide_ms;         /* emit cadence */

    struct timespec last_emit;     /* Last sink() */
} sliding_window_t;

/***********
 * Functions
 **********/

/**
 * @brief Initialize a sliding window.
 * @param w Pointer to the window.
 * @param window_size_ms Window span.
 * @param slide_ms Sliding interval.
 */
void sliding_window_init(sliding_window_t *w,
                         long window_size_ms,
                         long slide_ms);

/**
 * @brief Graceful cleanup of a window. Frees all ts_node_t in the window, not the window itself.
 * @param w Pointer to the window.
 */
void sliding_window_destroy(sliding_window_t *w);

/**
 * @brief Append the event timestamp as ts_node_t to tail. Evicts head while head->ts < (event_ts - window_size_ms).
 * @param w Pointer to the window.
 * @param event_ts Timestamp of the event.
 * @return 0 on success, -1 on alloc error, -2 on empty sliding window.
 */
int sliding_window_add(sliding_window_t *w, struct timespec event_ts);

/**
 * @brief Checks if it's time to emit (slide_ms elapsed since last emit).
 * @param w Pointer to the window.
 * @param now Current timestamp.
 * @return 1 if should emit, 0 otherwise.
 */
int sliding_window_should_emit(const sliding_window_t *w,
                                struct timespec now);

/**
 * @brief Called on emit, sets the last emit time.
 * @param w Pointer to the window.
 * @param now Current timestamp.
 */
void sliding_window_mark_emitted(sliding_window_t *w,
                                 struct timespec now);

/**
 * @brief Get the count of events currently in the window.
 * @param w Pointer to the window.
 * @return Number of events in the window.
 */
size_t sliding_window_count(const sliding_window_t *w);

/**
 * @brief Get the timestamp of the oldest event in the window.
 * @param w Pointer to the window.
 * @return Timespec of the oldest event.
 */
struct timespec sliding_window_oldest(const sliding_window_t *w);

/**
 * @brief Get the timestamp of the newest event in the window.
 * @param w Pointer to the window.
 * @return Timespec of the newest event.
 */
struct timespec sliding_window_newest(const sliding_window_t *w);
#endif
