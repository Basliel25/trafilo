#include "unity.h"
#include "../src/headers/socket.h"
#include "../src/headers/bounded_queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
    
 // 9998 = self-test (loopback, no external deps)
 // 9999 = matches the dummy daemon TARGET_PORT in docker-compose
#define SELF_TEST_PORT   9998
#define DAEMON_PORT      9999

void setUp(void) {}
void tearDown(void) {}

// Helper: send datagram to localhost:port
static int send_udp(const char *msg, int port) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &dest.sin_addr);

    ssize_t n = sendto(fd, msg, strlen(msg), 0,
                       (struct sockaddr *)&dest, sizeof(dest));
    close(fd);
    return (n < 0) ? -1 : 0;
}

// Lifecycle and sanity test: Limited queue size to 16
void test_listener_create_destroy(void) {
    bounded_queue_t *q = bq_create(16);
    listener_t *l = listener_create(SELF_TEST_PORT, q, 1024);

    TEST_ASSERT_NOT_NULL(l);

    listener_destroy(l);
    bq_destroy(q);
}

// End to end packet sanity
void test_single_packet_received(void) {
    bounded_queue_t *q = bq_create(16);
    listener_t *l = listener_create(SELF_TEST_PORT, q, 1024);
    TEST_ASSERT_NOT_NULL(l);

    TEST_ASSERT_EQUAL_INT(0, listener_start(l));

    /* wait listener thread to enter recvfrom */
    usleep(50000);   /* 50ms */

    TEST_ASSERT_EQUAL_INT(0, send_udp("hello trafilo", SELF_TEST_PORT));

    /* pop the line when the queue is full*/
    char *line = bq_pop(q);
    TEST_ASSERT_NOT_NULL(line);
    TEST_ASSERT_EQUAL_STRING("hello trafilo", line);
    free(line);

    listener_stop(l);
    listener_destroy(l);
    bq_shutdown(q);
    bq_destroy(q);
}

// Burst of bytes recieved in order

void test_burst_packets_received(void) {
    bounded_queue_t *q = bq_create(64);
    listener_t *l = listener_create(SELF_TEST_PORT, q, 1024);
    TEST_ASSERT_NOT_NULL(l);

    listener_start(l);
    usleep(50000);

    const int N = 20;
    char buf[64];
    for (int i = 0; i < N; i++) {
        snprintf(buf, sizeof(buf), "line-%d", i);
        send_udp(buf, SELF_TEST_PORT);
        usleep(1000);   
    }

    // Drain the queue
    int received = 0;
    for (int i = 0; i < N; i++) {
        char *line = bq_pop(q);
        if (line == NULL) break;
        received++;
        free(line);
    }

    // loopback UDP shouldn't drop unless under extreme load 
    TEST_ASSERT_TRUE_MESSAGE(received >= N - 2,
        "Lost too many loopback UDP packets");

    listener_stop(l);
    listener_destroy(l);
    bq_shutdown(q);
    bq_destroy(q);
}

// Shutdown and listener behavior
void test_shutdown_unblocks_listener(void) {
    bounded_queue_t *q = bq_create(16);
    listener_t *l = listener_create(SELF_TEST_PORT, q, 1024);
    TEST_ASSERT_NOT_NULL(l);

    listener_start(l);
    usleep(50000);   

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    listener_stop(l);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    long elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000
                    + (t1.tv_nsec - t0.tv_nsec) / 1000000;

    TEST_ASSERT_TRUE_MESSAGE(elapsed_ms < 500,
        "listener_stop took longer than 500ms — SO_RCVTIMEO not working?");

    listener_destroy(l);
    bq_shutdown(q);
    bq_destroy(q);
}

// Test with the dummy dameon on port: 9999
void test_live_daemon_monitor(void) {
    bounded_queue_t *q = bq_create(1024);
    listener_t *l = listener_create(DAEMON_PORT, q, 2048);

    if (l == NULL) {
        TEST_IGNORE_MESSAGE("Port 9999 unavailable or busy");
        bq_destroy(q);
        return;
    }

    listener_start(l);
    printf("\n  [live] listening on UDP %d...\n", DAEMON_PORT);

    // Monitoring loop: with a simple timeout to pop elements
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += 5;

    long lines_received = 0;
    long bytes_received = 0;
    long first_arrival_ms = -1;
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    // Drain the queue 
    while (1) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > deadline.tv_sec ||
            (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec)) {
            break;
        }

        // signal listener+queue to wake any blocked pop after 5s
        usleep(100000);   /* 100ms tick */
    }

    // shut down listener so bq_pop won't block forever 
    listener_stop(l);
    bq_shutdown(q);

    // drain everything recieved
    char *line;
    while ((line = bq_pop(q)) != NULL) {
        if (first_arrival_ms < 0) {
            struct timespec t;
            clock_gettime(CLOCK_MONOTONIC, &t);
            first_arrival_ms = (t.tv_sec - t0.tv_sec) * 1000
                             + (t.tv_nsec - t0.tv_nsec) / 1000000;
        }
        lines_received++;
        bytes_received += strlen(line);
        if (lines_received <= 3) {
            printf("  [live] sample: %.80s%s\n", line,
                   strlen(line) > 80 ? "..." : "");
        }
        free(line);
    }

    printf("  [live]  ==stats== \n");
    printf("  [live] Lines received: %ld\n", lines_received);
    printf("  [live] NUM_bytes received: %ld\n", bytes_received);
    if (lines_received > 0) {
        printf("  [live] average line size: %ld bytes\n", bytes_received / lines_received);
        printf("  [live] recive rate:          %.1f lines/sec\n", lines_received / 5.0);
    } else {
        printf("  [live] (no packets recived \n");
        TEST_IGNORE_MESSAGE("No packets received on 9999.\n");
    }

    listener_destroy(l);
    bq_destroy(q);
}

int main(void) {
    UNITY_BEGIN();

    //RUN_TEST(test_listener_create_destroy);
    //RUN_TEST(test_single_packet_received);
    //RUN_TEST(test_burst_packets_received);
    //RUN_TEST(test_shutdown_unblocks_listener);
    //RUN_TEST(test_live_daemon_monitor);

    return UNITY_END();
}
