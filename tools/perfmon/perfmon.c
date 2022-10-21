#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <trace/trace.h>

#define ee(...){\
	int errno__ = errno;\
	printf("(ee) " __VA_ARGS__);\
	printf("(ee) %s | %s:%d\n", strerror(errno__), __func__, __LINE__);\
	errno = errno__;\
}

#define ii(...) printf("(ii) " __VA_ARGS__)

static char label_[32];
static uint32_t period_ = 5000; /* microseconds */

#define GPU_PERF_ENABLE "/sys/kernel/debug/gpu.0/perfmon_events_enable"

static uint8_t enable_perf_events(char flag)
{
	FILE *f = fopen(GPU_PERF_ENABLE, "w");
	if (!f) {
		ee("failed to open '" GPU_PERF_ENABLE "' file");
		return 0;
	}

	if (fprintf(f, "%c", flag) < 0) {
		ee("failed to write 1 byte to '" GPU_PERF_ENABLE "'");
		return 0;
	}

	fclose(f);
	return 1;
}

#define GPU_PERF "/sys/kernel/debug/gpu.0/perfmon_events_count"

static void gpuperf_mon(void)
{
	if (!enable_perf_events(1))
		return;

	while (1) {
		FILE *f = fopen(GPU_PERF, "r");
		if (!f) {
			ee("failed to open '" GPU_PERF "' file");
			break;
		}

		size_t counter;
		if (fscanf(f, "%zu", &counter) == EOF && errno) {
			ee("'" GPU_PERF "' file reading error\n");
			sleep(1);
		}

		fclose(f);
		trace_marker_mon("%zu,%s", counter, label_);
		usleep(period_);
	}

	enable_perf_events(0);
}

#define GPU_STATUS "/sys/kernel/debug/gpu.0/status"

static void gpustat_mon(void)
{
	FILE *f = fopen(GPU_STATUS, "r");
	if (!f) {
		ee("failed to open '" GPU_STATUS "' file");
		return;
	}

	fclose(f);
}

#define GPU_LOAD "/sys/devices/gpu.0/load"

static void gpuload_mon(void)
{
	while (1) {
		FILE *f = fopen(GPU_LOAD, "r");
		if (!f) {
			ee("failed to open '" GPU_LOAD "' file");
			return;
		}

		size_t counter;
		if (fscanf(f, "%zu", &counter) == EOF && errno) {
			ee("'" GPU_LOAD "' file reading error\n");
			sleep(1);
		}

		fclose(f);
		trace_marker_mon("%zu,%s", counter, label_);
		usleep(period_);
	}
}

#define GPU_FREQ "/sys/kernel/debug/bpmp/debug/clk/gpcclk/rate"

static void gpufreq_mon(void)
{
	while (1) {
		FILE *f = fopen(GPU_FREQ, "r");
		if (!f) {
			ee("failed to open '" GPU_FREQ "' file");
			return;
		}

		size_t counter;
		if (fscanf(f, "%zu", &counter) == EOF && errno) {
			ee("'" GPU_FREQ "' file reading error\n");
			sleep(1);
		}
		fclose(f);
		trace_marker_mon("%zu,%s", counter, label_);
		usleep(period_);
	}
}

#define EMC_FREQ "/sys/kernel/debug/bpmp/debug/clk/emc/rate"

static void emcfreq_mon(void)
{
	while (1) {
		FILE *f = fopen(EMC_FREQ, "r");
		if (!f) {
			ee("failed to open '" EMC_FREQ "' file");
			return;
		}

		size_t counter;
		if (fscanf(f, "%zu", &counter) == EOF && errno) {
			ee("'" EMC_FREQ "' file reading error\n");
			sleep(1);
		}

		fclose(f);
		trace_marker_mon("%zu,%s", counter, label_);
		usleep(period_);
	}
}

#define EMC_LOAD "/sys/kernel/actmon_avg_activity/mc_all"

static void emcload_mon(void)
{
	while (1) {
		FILE *f = fopen(EMC_LOAD, "r");
		if (!f) {
			ee("failed to open '" EMC_LOAD "' file");
			return;
		}

		size_t counter;
		if (fscanf(f, "%zu", &counter) == EOF && errno) {
			ee("'" EMC_LOAD "' file reading error\n");
			sleep(1);
		}

		fclose(f);
		trace_marker_mon("%zu,%s", counter, label_);
		usleep(period_);
	}
}

static void help(const char *name)
{
	printf("Usage: create and run symlink with desired function\n"
	 "Available monitors:\n"
         "\033[2m"
	 " gpuperf-mon  for GPU performance monitoring counter\n"
	 " gpuload-mon  for GPU load counter\n"
	 " gpufreq-mon  for GPU frequency\n"
	 " emcload-mon  for EMC load counter (external memory controller)\n"
	 " emcfreq-mon  for EMC frequency\n"
	 "\033[0m"
	 "Example:\n"
	 " ~/> ln -sf %s gpuperf-mon\n"
	 " ~/> ./gpuperf-mon --label MHz\n", name);
}

static int opt(const char *arg, const char *argl)
{
	return (strcmp(arg, argl) == 0);
}

static void getopts(int argc, const char *argv[])
{
	const char *arg;

	for (uint8_t i = 0; i < argc; ++i) {
		arg = argv[i];
		if (opt(arg, "--label")) {
			i++;
			strncpy(label_, argv[i], sizeof(label_) - 1);
			ii("Using label '%s'\n", label_);
		} else if (opt(arg, "--period-us")) {
			i++;
			period_ = atoi(argv[i]);
			ii("Using period %d us\n", period_);
		}
	}
}

typedef void (*monitor_fn)(void);

int main(int argc, const char *argv[])
{
	monitor_fn fn = NULL;

	if (strstr(argv[0], "gpuperf-mon")) {
		fn = gpuperf_mon;
	} else if (strstr(argv[0], "gpuload-mon")) {
		fn = gpuload_mon;
	} else if (strstr(argv[0], "gpufreq-mon")) {
		fn = gpufreq_mon;
	} else if (strstr(argv[0], "emcload-mon")) {
		fn = emcload_mon;
	} else if (strstr(argv[0], "emcfreq-mon")) {
		fn = emcfreq_mon;
	} else {
		help(argv[0]);
		return 0;
	}

	getopts(argc, argv);

	pid_t pid;
	if ((pid = fork()) < 0) {
		return 1;
	} else if (pid == 0) {  // child process
		fn();
	}

	return 0;
}
