#define _GNU_SOURCE

#include "metrics.h"

VideoInfo* process_pages_and_extract_data(Page* pages,size_t num_pages, size_t* num_lines,size_t max_lines) {
    *num_lines = 0;
    long* line_positions = (long*)malloc(max_lines * sizeof(long));
    VideoInfo* video_info_array = (VideoInfo*)malloc(max_lines * sizeof(VideoInfo));
    
    if (!line_positions || !video_info_array) {
        fprintf(stderr, "Memory allocation failed\n");
        free(line_positions);
        free(video_info_array);
        return NULL;
    }

    line_positions[*num_lines] = 0;
    (*num_lines)++;
    // Process each page
    for (size_t i = 0; i < num_pages; ++i) {
        for (size_t j = 0; j < pages[i].size; ++j) {
            if (pages[i].data[j] == '\n') {
                if (*num_lines >= max_lines) {
                    max_lines *= 2;
                    long* new_positions = (long*)realloc(line_positions, max_lines * sizeof(long));
                    VideoInfo* new_info = (VideoInfo*)realloc(video_info_array, max_lines * sizeof(VideoInfo));
                    
                    if (!new_positions || !new_info) {
                        fprintf(stderr, "Reallocation failed\n");
                        free(line_positions);
                        free(video_info_array);
                        return NULL;
                    }
                    
                    line_positions = new_positions;
                    video_info_array = new_info;
                }

                line_positions[*num_lines] = (i * PAGE_SIZE) + j + 1;
                
                // Extract line safely
                long start_pos = line_positions[*num_lines - 1];
                long end_pos = line_positions[*num_lines] - 1;
                size_t line_length = end_pos - start_pos + 1;
                
                char* line = (char*)malloc(line_length + 1);
                if (!line) {
                    fprintf(stderr, "Memory allocation failed for line buffer\n");
                    continue;
                }

                // Copy line data safely
                size_t copied = 0;
                size_t start_page = start_pos / PAGE_SIZE;
                size_t start_offset = start_pos % PAGE_SIZE;
                size_t end_page = end_pos / PAGE_SIZE;
                
                // Copy from first page
                size_t first_page_bytes = (start_page == end_page) ? 
                    line_length : PAGE_SIZE - start_offset;
                memcpy(line, &pages[start_page].data[start_offset], first_page_bytes);
                copied += first_page_bytes;

                // Copy from subsequent pages if line spans multiple pages
                for (size_t p = start_page + 1; p <= end_page && copied < line_length; ++p) {
                    size_t bytes_to_copy = (p == end_page) ? 
                        (end_pos % PAGE_SIZE) + 1 : PAGE_SIZE;
                    if (copied + bytes_to_copy > line_length) {
                        bytes_to_copy = line_length - copied;
                    }
                    memcpy(line + copied, pages[p].data, bytes_to_copy);
                    copied += bytes_to_copy;
                }
                
                line[line_length] = '\0';

                // Parse CSV fields safely
                char* token = strtok(line, ",");
                char* title = NULL;
                long views = 0;
                int field = 0;
                
                while (token && field < 8) {
                    if (field == 2) {  // Title field
                        title = token;
                    } else if (field == 7) {  // Views field
                        views = atol(token);
                    }
                    token = strtok(NULL, ",");
                    field++;
                }

                // Store the information safely
                if (title) {
                    strncpy(video_info_array[*num_lines - 1].title, title, 
                        sizeof(video_info_array[*num_lines - 1].title) - 1);
                    video_info_array[*num_lines - 1].title[sizeof(video_info_array[*num_lines - 1].title) - 1] = '\0';
                } else {
                    video_info_array[*num_lines - 1].title[0] = '\0';
                }
                video_info_array[*num_lines - 1].views = views;

                free(line);
                (*num_lines)++;
            }
        }
    }
     
    return video_info_array; 
}

char* extract_most_viewed_video_info(VideoInfo* videos_info, size_t num_lines) {
    uint32_t max_views = 0;
    long line_most_viewed = 0;

    char* most_viewed_title = (char*)malloc(256 * sizeof(char)); // Array to store the title of the most viewed video
    if (!most_viewed_title) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    for (size_t i = 1; i < num_lines - 1; ++i) {  // Start from 1 to skip header
        if (videos_info[i].views > max_views) {
            max_views = videos_info[i].views;
            strncpy(most_viewed_title, videos_info[i].title, 255);
            most_viewed_title[255] = '\0';  // Ensure null termination
            line_most_viewed = i;
        }
    }

    if (max_views > 0) {
        return most_viewed_title;
    } else {
        printf("No videos found.\n");
        free(most_viewed_title);  // Free the memory if no valid result
        return NULL;
    }
}

void* find_most_viewed_video(void* arg) {
    ThreadData *data = (ThreadData *)arg;
    long local_max_views = 0;
    char local_most_viewed_title[256] = {0};

    // Process the assigned chunk of videos
    for (size_t i = data->start; i < data->end; ++i) {
        if (data->videos_info[i].views > local_max_views) {
            local_max_views = data->videos_info[i].views;
            strncpy(local_most_viewed_title, data->videos_info[i].title, 128);
            local_most_viewed_title[128] = '\0';  // Ensure null termination
        }
    }

    // Update the global max views and most viewed title safely
    pthread_mutex_lock(data->mutex);
    if (local_max_views > *data->max_views) {
        *data->max_views = local_max_views;
        strncpy(data->most_viewed_title, local_most_viewed_title, 128);
        data->most_viewed_title[128] = '\0';
    }
    pthread_mutex_unlock(data->mutex);
    pthread_exit(NULL);
}

void divide_and_process_chunks(VideoInfo *video_info_array, char** most_viewed_str ,size_t* most_views ,size_t num_lines) {
    size_t num_threads = num_lines / LINES_PER_THREAD;
    
    pthread_t threads[num_threads];
    ThreadData thread_data[num_lines];
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    uint32_t max_views = 0;
    char most_viewed_title[256] = {0};

    // size_t chunk_size = num_lines / num_threads;
    
  
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 1024 * 1024);

    // Create threads and assign chunks
    for (int i = 0; i < num_threads; ++i) {
        thread_data[i].videos_info = video_info_array;
        thread_data[i].start = i * LINES_PER_THREAD;
        thread_data[i].end = (i == num_threads - 1) ? num_lines : (i + 1) * LINES_PER_THREAD;
        thread_data[i].max_views = &max_views;
        thread_data[i].most_viewed_title = most_viewed_title;
        thread_data[i].mutex = &mutex;

        pthread_create(&threads[i], &attr, find_most_viewed_video, (void*)&thread_data[i]);
    }
    // Join threads
    for (int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }
    pthread_attr_destroy(&attr);
    // Print the final result
    (*most_views) = max_views;
    strncpy(*most_viewed_str, most_viewed_title, 255);
    (*most_viewed_str)[255] = '\0';  // Ensure null termination
}
