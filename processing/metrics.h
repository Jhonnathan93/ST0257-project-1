#ifndef METRICS_H
#define METRICS_H

// Global libraries
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>


// Local libraries
#include "../utils/constants.h"
#include "../utils/structs.h"
#include "../utils/utils.h"

// Function prototypes
VideoInfo* process_pages_and_extract_data(Page* pages,size_t num_pages, size_t* num_videos, size_t max_videos);
char* extract_most_viewed_video(VideoInfo* videos, size_t num_videos);
void* find_most_viewed_video(void* arg);
void divide_and_process_chunks(VideoInfo* videos, char** most_viewed_title, size_t* most_views, size_t num_videos);

#endif // METRICS_H
