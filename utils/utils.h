#ifndef UTILS_H
#define UTILS_H

#include <dirent.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <stdlib.h>

void set_cpu_affinity(int cpu);
char** get_csv_file_list(const char* directory, int* num_files);
long get_memory_usage(pid_t pid);
void print_array(long* arr, size_t size);

#endif
