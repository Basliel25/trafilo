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

int main(void) {
    UNITY_BEGIN();

    //RUN_TEST(test_listener_create_destroy);
    //RUN_TEST(test_single_packet_received);
    //RUN_TEST(test_burst_packets_received);
    //RUN_TEST(test_shutdown_unblocks_listener);
    //RUN_TEST(test_live_daemon_monitor);

    return UNITY_END();
}
