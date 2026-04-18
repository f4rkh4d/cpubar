/*
 * cpubar - tiny per-core cpu load bars
 * single file, no deps. macos + linux.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#define CPUBAR_VERSION "0.1.0"
#define MAX_CORES 1024
#define DEFAULT_WIDTH 20
#define DEFAULT_INTERVAL 1.0

#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/processor_info.h>
#include <mach/host_info.h>
#endif

static volatile sig_atomic_t g_stop = 0;

static void on_sigint(int sig) {
    (void)sig;
    g_stop = 1;
}

typedef struct {
    uint64_t used;
    uint64_t total;
} core_sample_t;

/* fills samples[] with per-core cumulative counters, returns count or -1 */
static int read_samples(core_sample_t *samples, int max) {
#if defined(__APPLE__)
    natural_t ncpu = 0;
    processor_info_array_t info = NULL;
    mach_msg_type_number_t icount = 0;
    kern_return_t kr = host_processor_info(mach_host_self(),
                                           PROCESSOR_CPU_LOAD_INFO,
                                           &ncpu,
                                           &info,
                                           &icount);
    if (kr != KERN_SUCCESS) return -1;
    int n = (int)ncpu;
    if (n > max) n = max;
    processor_cpu_load_info_t loads = (processor_cpu_load_info_t)info;
    for (int i = 0; i < n; i++) {
        uint64_t user = loads[i].cpu_ticks[CPU_STATE_USER];
        uint64_t sys  = loads[i].cpu_ticks[CPU_STATE_SYSTEM];
        uint64_t nice = loads[i].cpu_ticks[CPU_STATE_NICE];
        uint64_t idle = loads[i].cpu_ticks[CPU_STATE_IDLE];
        samples[i].used = user + sys + nice;
        samples[i].total = user + sys + nice + idle;
    }
    vm_deallocate(mach_task_self(), (vm_address_t)info, icount * sizeof(*info));
    return n;
#else
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return -1;
    char line[512];
    int n = 0;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "cpu", 3) != 0) continue;
        if (line[3] < '0' || line[3] > '9') continue; /* skip aggregate "cpu " */
        if (n >= max) break;
        unsigned long long user, nice, sys, idle, iowait, irq, softirq, steal;
        user = nice = sys = idle = iowait = irq = softirq = steal = 0;
        sscanf(line + 4, "%*d %llu %llu %llu %llu %llu %llu %llu %llu",
               &user, &nice, &sys, &idle, &iowait, &irq, &softirq, &steal);
        uint64_t used = user + nice + sys + irq + softirq + steal;
        uint64_t total = used + idle + iowait;
        samples[n].used = used;
        samples[n].total = total;
        n++;
    }
    fclose(f);
    return n;
#endif
}

static void print_bar(int core, double pct, int width) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    int filled = (int)((pct / 100.0) * width + 0.5);
    if (filled > width) filled = width;
    printf("core %-2d ", core);
    for (int i = 0; i < filled; i++) fputs("\xe2\x96\x87", stdout); /* ▇ */
    for (int i = filled; i < width; i++) fputs("\xe2\x96\x91", stdout); /* ░ */
    printf("  %3d%%\n", (int)(pct + 0.5));
}

static void sleep_seconds(double s) {
    if (s <= 0) return;
    struct timespec ts;
    ts.tv_sec = (time_t)s;
    ts.tv_nsec = (long)((s - (double)ts.tv_sec) * 1e9);
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
        if (g_stop) return;
    }
}

static void usage(FILE *out) {
    fputs(
        "usage: cpubar [-i SECONDS] [-n ITERATIONS] [-w WIDTH] [--help] [--version]\n"
        "  -i SECONDS     refresh interval (default 1.0)\n"
        "  -n ITERATIONS  run N times then exit (default: forever)\n"
        "  -w WIDTH       bar width in chars (default 20)\n"
        "  --help         show this message\n"
        "  --version      show version\n",
        out);
}

int main(int argc, char **argv) {
    double interval = DEFAULT_INTERVAL;
    int iterations = -1; /* forever */
    int width = DEFAULT_WIDTH;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            usage(stdout);
            return 0;
        } else if (strcmp(a, "--version") == 0 || strcmp(a, "-V") == 0) {
            printf("cpubar %s\n", CPUBAR_VERSION);
            return 0;
        } else if (strcmp(a, "-i") == 0 && i + 1 < argc) {
            interval = atof(argv[++i]);
            if (interval <= 0) { fprintf(stderr, "cpubar: bad -i\n"); return 2; }
        } else if (strcmp(a, "-n") == 0 && i + 1 < argc) {
            iterations = atoi(argv[++i]);
            if (iterations <= 0) { fprintf(stderr, "cpubar: bad -n\n"); return 2; }
        } else if (strcmp(a, "-w") == 0 && i + 1 < argc) {
            width = atoi(argv[++i]);
            if (width < 1 || width > 200) { fprintf(stderr, "cpubar: bad -w\n"); return 2; }
        } else {
            fprintf(stderr, "cpubar: unknown arg '%s'\n", a);
            usage(stderr);
            return 2;
        }
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_sigint;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    core_sample_t prev[MAX_CORES];
    core_sample_t cur[MAX_CORES];

    int n = read_samples(prev, MAX_CORES);
    if (n < 0) {
        fprintf(stderr, "cpubar: failed to read cpu stats\n");
        return 1;
    }

    /* small warmup so first frame isn't all zeros */
    sleep_seconds(interval);
    if (g_stop) { printf("\n"); return 0; }

    int count = 0;
    while (!g_stop) {
        int m = read_samples(cur, MAX_CORES);
        if (m < 0) {
            fprintf(stderr, "cpubar: read failed\n");
            return 1;
        }
        int cores = (m < n) ? m : n;
        for (int i = 0; i < cores; i++) {
            uint64_t du = cur[i].used  - prev[i].used;
            uint64_t dt = cur[i].total - prev[i].total;
            double pct = (dt > 0) ? (100.0 * (double)du / (double)dt) : 0.0;
            print_bar(i, pct, width);
        }
        printf("(ctrl+c to quit)\n");
        fflush(stdout);

        memcpy(prev, cur, sizeof(core_sample_t) * (size_t)cores);
        n = m;

        count++;
        if (iterations > 0 && count >= iterations) break;

        sleep_seconds(interval);
    }

    /* graceful line-clear on exit */
    printf("\n");
    return 0;
}
