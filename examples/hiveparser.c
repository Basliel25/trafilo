/*
 * hiveparser.c — top-N log analyzer demo on trafilo
 *
 * UDP log ingest to per-service event/error counters to ncurses dashboard.
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <../include/trafilo.h>

#include <ncurses.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

/* ── tunables ─────────────────────────────────────────────────── */

#define MAX_SERVICES   64
#define HP_KEY_MAX     63
#define UDP_PORT       9999
#define NUM_WORKERS    4
#define NUM_BUCKETS    127
#define WINDOW_MS      5000
#define SLIDE_MS       500
#define REDRAW_MS      250

/* ── shared TUI snapshot ──────────────────────────────────────── */

typedef struct {
    char            key[HP_KEY_MAX + 1];
    size_t          total_count;
    size_t          error_count;
    size_t          window_count;
    struct timespec last_seen;
    int             in_use;
} tui_row_t;

typedef struct {
    pthread_mutex_t lock;
    tui_row_t       rows[MAX_SERVICES];
    volatile int    shutdown;
} tui_state_t;

static tui_state_t  g_tui;
static trafilo_t   *g_trafilo = NULL;
static pthread_t    g_tui_thread;

/* find or insert a row for key. caller MUST hold g_tui.lock. */
static tui_row_t *tui_row_get(const char *key) {
    int free_slot = -1;
    for (int i = 0; i < MAX_SERVICES; i++) {
        if (g_tui.rows[i].in_use) {
            if (strncmp(g_tui.rows[i].key, key, HP_KEY_MAX) == 0)
                return &g_tui.rows[i];
        } else if (free_slot < 0) {
            free_slot = i;
        }
    }
    if (free_slot < 0) return NULL; /* table full, drop silently */

    tui_row_t *r = &g_tui.rows[free_slot];
    memset(r, 0, sizeof(*r));
    strncpy(r->key, key, HP_KEY_MAX);
    r->key[HP_KEY_MAX] = '\0';
    r->in_use = 1;
    return r;
}

/* ── per-bucket state ─────────────────────────────────────────── */

typedef struct {
    size_t total;
    size_t errors;
} service_state_t;

/* error vocabulary — matches dummy_dameon.sh classifier */
static int looks_like_error(const char *s, size_t len) {
    static const char *needles[] = {
        "error", "fail", "fatal", "denied", "refused",
        "invalid", "unable", "break-in", "abnormal", NULL
    };
    for (int i = 0; needles[i]; i++) {
        size_t nlen = strlen(needles[i]);
        if (nlen > len) continue;
        for (size_t j = 0; j + nlen <= len; j++) {
            if (strncasecmp(s + j, needles[i], nlen) == 0) return 1;
        }
    }
    return 0;
}

/* ── trafilo callbacks ────────────────────────────────────────── */

static int parse_fn(const char *raw, size_t len, event_t **out) {
    const char *space = memchr(raw, ' ', len);
    if (space == NULL || space == raw) return -1;
    size_t key_len = space - raw;
    if (key_len > HP_KEY_MAX) key_len = HP_KEY_MAX;

    event_t *e = malloc(sizeof(event_t));
    if (e == NULL) return -1;

    e->key = malloc(key_len + 1);
    if (e->key == NULL) { free(e); return -1; }
    memcpy(e->key, raw, key_len);
    e->key[key_len] = '\0';

    /* payload points into raw — valid only during handle() */
    e->payload     = (void *)(space + 1);
    e->payload_len = len - key_len - 1;
    clock_gettime(CLOCK_MONOTONIC, &e->t_secs);

    *out = e;
    return 0;
}

static void event_free_fn(event_t *e) {
    if (e == NULL) return;
    free(e->key);
    free(e);
}

static void handle_fn(const event_t *e, void *user_state) {
    service_state_t *s = (service_state_t *)user_state;
    s->total++;
    if (looks_like_error((const char *)e->payload, e->payload_len)) {
        s->errors++;
    }
}

static void *state_init_fn(const char *key) {
    (void)key;
    return calloc(1, sizeof(service_state_t));
}

static void state_free_fn(void *s) { free(s); }

/* sink: copy snapshot into the TUI table. NO ncurses calls here. */
static void sink_fn(const char *key,
                    const window_result_t *result,
                    void *user_state) {
    service_state_t *s = (service_state_t *)user_state;

    pthread_mutex_lock(&g_tui.lock);
    tui_row_t *row = tui_row_get(key);
    if (row) {
        row->total_count  = s->total;
        row->error_count  = s->errors;
        row->window_count = result->event_count;
        clock_gettime(CLOCK_MONOTONIC, &row->last_seen);
    }
    pthread_mutex_unlock(&g_tui.lock);
}

/* ── TUI rendering ────────────────────────────────────────────── */

static int row_cmp_desc(const void *a, const void *b) {
    const tui_row_t *ra = a, *rb = b;
    if (rb->total_count > ra->total_count) return  1;
    if (rb->total_count < ra->total_count) return -1;
    return 0;
}

static long secs_since(struct timespec then) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec - then.tv_sec;
}

static void render(void) {
    tui_row_t snapshot[MAX_SERVICES];
    int n = 0;
    size_t total_events = 0;

    pthread_mutex_lock(&g_tui.lock);
    for (int i = 0; i < MAX_SERVICES; i++) {
        if (g_tui.rows[i].in_use) snapshot[n++] = g_tui.rows[i];
    }
    pthread_mutex_unlock(&g_tui.lock);

    qsort(snapshot, n, sizeof(tui_row_t), row_cmp_desc);
    for (int i = 0; i < n; i++) total_events += snapshot[i].total_count;

    erase();

    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);

    attron(A_BOLD);
    mvprintw(0, 2, "hiveparser  —  trafilo streaming demo");
    mvprintw(0, COLS - 12, "%02d:%02d:%02d",
             tm.tm_hour, tm.tm_min, tm.tm_sec);
    attroff(A_BOLD);

    mvhline(1, 0, ACS_HLINE, COLS);

    attron(A_BOLD | A_UNDERLINE);
    mvprintw(3, 2,  "SERVICE");
    mvprintw(3, 24, "EVENTS");
    mvprintw(3, 36, "WINDOW");
    mvprintw(3, 48, "ERRORS");
    mvprintw(3, 60, "ERR%%");
    mvprintw(3, 70, "LAST SEEN");
    attroff(A_BOLD | A_UNDERLINE);

    int row_y = 5;
    int top_n = n < (LINES - 8) ? n : (LINES - 8);
    for (int i = 0; i < top_n; i++) {
        tui_row_t *r = &snapshot[i];
        double err_pct = r->total_count
            ? 100.0 * (double)r->error_count / (double)r->total_count : 0.0;

        int color = 1; /* green */
        if (err_pct >= 5.0)  color = 2; /* yellow */
        if (err_pct >= 10.0) color = 3; /* red */

        mvprintw(row_y, 2,  "%-20.20s", r->key);
        mvprintw(row_y, 24, "%-10zu",  r->total_count);
        mvprintw(row_y, 36, "%-10zu",  r->window_count);
        mvprintw(row_y, 48, "%-10zu",  r->error_count);

        attron(COLOR_PAIR(color));
        mvprintw(row_y, 60, "%5.1f%%", err_pct);
        attroff(COLOR_PAIR(color));

        long secs = secs_since(r->last_seen);
        mvprintw(row_y, 70, "%lds ago", secs);

        row_y++;
    }

    mvhline(LINES - 3, 0, ACS_HLINE, COLS);
    mvprintw(LINES - 2, 2,
             "total: %zu events  •  %d services  •  port %d  •  q=quit",
             total_events, n, UDP_PORT);

    refresh();
}

static void *tui_loop(void *arg) {
    (void)arg;

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_GREEN,  -1);
        init_pair(2, COLOR_YELLOW, -1);
        init_pair(3, COLOR_RED,    -1);
    }

    while (!g_tui.shutdown) {
        int ch = getch();
        if (ch == 'q' || ch == 'Q') {
            trafilo_shutdown(g_trafilo);
            break;
        }
        render();
        usleep(REDRAW_MS * 1000);
    }

    endwin();
    return NULL;
}

/* ── signals ──────────────────────────────────────────────────── */

static void on_sigint(int sig) {
    (void)sig;
    g_tui.shutdown = 1;
    if (g_trafilo) trafilo_shutdown(g_trafilo);
}

/* ── main ─────────────────────────────────────────────────────── */

int main(void) {
    memset(&g_tui, 0, sizeof(g_tui));
    pthread_mutex_init(&g_tui.lock, NULL);

    struct sigaction sa = { .sa_handler = on_sigint };
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    trafilo_config_t cfg = {
        .bind_addr              = "0.0.0.0",
        .port                   = UDP_PORT,
        .recv_timeout_ms        = 100,
        .num_workers            = NUM_WORKERS,
        .num_buckets            = NUM_BUCKETS,
        .window_size_ms         = WINDOW_MS,
        .slide_interval_ms      = SLIDE_MS,
        .bucket_idle_timeout_ms = 0,
        .parse       = parse_fn,
        .event_free  = event_free_fn,
        .handle      = handle_fn,
        .sink        = sink_fn,
        .state_init  = state_init_fn,
        .state_free  = state_free_fn,
    };

    g_trafilo = trafilo_create(&cfg);
    if (g_trafilo == NULL) {
        fprintf(stderr, "trafilo_create failed\n");
        return 1;
    }

    if (pthread_create(&g_tui_thread, NULL, tui_loop, NULL) != 0) {
        fprintf(stderr, "tui thread spawn failed\n");
        trafilo_destroy(g_trafilo);
        return 1;
    }

    int rc = trafilo_run(g_trafilo);

    g_tui.shutdown = 1;
    pthread_join(g_tui_thread, NULL);

    trafilo_destroy(g_trafilo);
    pthread_mutex_destroy(&g_tui.lock);

    return rc;
}
