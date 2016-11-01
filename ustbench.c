#define _GNU_SOURCE
#include <pthread.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "ustbench.h"
#include "lightweight-ust.h"
#include "libustperf.h"

/* Defines whether we track misses (1) or branches (0) */
#define TRACK_PMU_MISSES   1

#define TRACEPOINT_DEFINE
#include "tp.h"
#include <lttng/tracef.h>


void (*do_tp)(size_t size);
char *open_filename;

static inline void do_lttng_ust_tp(size_t size)
{
    tracepoint(TRACEPOINT_PROVIDER, bench_tp_4b, 0);
}

static inline void do_tracef(size_t size)
{
    int i = 0;
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    tracef("__%lu %lu %d", ts.tv_sec, ts.tv_nsec, i);
}

static inline void do_lightweight_ust_tp(size_t size)
{
    int i = 0;

    trace_record_write(&i, sizeof(i));
}

static inline void do_printf_tp(size_t size)
{
    int i = 0;
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    fprintf(stdout, "__%lu %lu %d", ts.tv_sec, ts.tv_nsec, i);
}

static inline void do_none_tp(size_t size)
{
}

static inline void do_open(size_t fd)
{
    open(open_filename, O_RDONLY);
}

void *do_work(void *a)
{
    ustperf_do_work(do_tp, a);

    return 0;
}

void init_work_function()
{
    int fd;

    if(strcmp(popt_args.tracer, "lw-ust") == 0) {
        printf("Setting tracer lw-ust\n");
        do_tp = do_lightweight_ust_tp;

    } else if (strcmp(popt_args.tracer, "printf") == 0) {
        printf("Setting tracer printf\n");

        fd = open("/dev/null", O_WRONLY);
        dup2(fd, 1);
        do_tp = do_printf_tp;
        close(fd);

    } else if (strcmp(popt_args.tracer, "none") == 0 ||
               strcmp(popt_args.tracer, "stap-ust") == 0) {
        printf("Setting tracer none\n");
        do_tp = do_none_tp;

    } else if(strcmp(popt_args.tracer, "open") == 0) {
        printf("Setting tracer open\n");
        int len = popt_args.len;
        open_filename = malloc(len);
        strncpy(open_filename,
                "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz", len);
        do_tp = do_open;

    } else if(strcmp(popt_args.tracer, "lttng-tracef") == 0) {
        printf("Setting tracer tracef\n");
        do_tp = do_tracef;

    } else {
        printf("Setting tracer lttng-ust\n");
        do_tp = do_lttng_ust_tp;
    }
}

int main(int argc, char **argv)
{
    int i, nCpus;
    struct libustperf_args *worker_args;
    pthread_t *threads;
    poptContext pc;

    popt_args.tracer = "lttng-ust";
    popt_args.loops = 10;
    popt_args.nthreads = 1;

    nCpus = sysconf(_SC_NPROCESSORS_ONLN);
    parse_args(argc, argv, &pc);
    threads = (pthread_t*) malloc(popt_args.nthreads * sizeof(pthread_t));
    worker_args = (struct libustperf_args*)
            malloc(popt_args.nthreads * sizeof(struct libustperf_args));

    perf_init(nCpus);
    init_work_function();

    for(i = 0; i < popt_args.nthreads; i++) {
        worker_args[i].id = i;
        worker_args[i].loops = popt_args.loops;
        printf("nthreads = %d\n", popt_args.nthreads);
        /* Set CPU affinity for each thread*/
        cpu_set_t cpu_set;
        pthread_attr_t attr;
        CPU_ZERO(&cpu_set);
        CPU_SET(i % nCpus, &cpu_set);
        pthread_attr_init(&attr);
        pthread_attr_setaffinity_np(&attr, sizeof(cpu_set), &cpu_set);
        printf("loops = %d\n", worker_args->loops);

        pthread_create(&threads[i], &attr, do_work, (void*) &worker_args[i]);
    }

    for(i = 0; i < popt_args.nthreads; i++) {
        pthread_join(threads[i], NULL);
    }

    output_measurements(nCpus);

    return 0;
}
