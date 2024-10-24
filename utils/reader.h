#ifndef READER_H
#define READER_H

#include <stdio.h>
#include <sys/time.h>
#include <sys/wait.h>


#include "constants.h"
#include "utils.h"
#include "structs.h"
#include "../processing/metrics.h"

void read_file(const char* filename, int is_main_process, int write_fd);
void read_pages(const char* filename, Page** pages, size_t* num_pages);

#endif // READER_H
