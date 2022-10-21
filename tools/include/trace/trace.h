#ifndef TRACE_H_
#define TRACE_H_

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define trace_marker(...) {\
  FILE *trace_file_ = fopen("/sys/kernel/tracing/trace_marker", "w");\
  if (trace_file_) {\
    fprintf(trace_file_, __VA_ARGS__);\
    fclose(trace_file_);\
  }\
}

#define trace_marker_mon(...) {\
  FILE *trace_file_ = fopen("/sys/kernel/tracing/trace_marker", "w");\
  if (trace_file_) {\
    fprintf(trace_file_, "mon," __VA_ARGS__);\
    fclose(trace_file_);\
  }\
}

#endif /* TRACE_H_ */
