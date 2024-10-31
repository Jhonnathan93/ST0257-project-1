#ifndef STRUCTS_H
#define STRUCTS_H

#include <pthread.h>
#include <stdint.h>

typedef struct Page {
    char *data;
    uint16_t size;
} Page;

typedef struct {
    char title[128];
    uint32_t views;
} VideoInfo;

typedef struct {
    VideoInfo *videos_info;
    size_t start;
    size_t end;
    uint32_t *max_views;
    char *most_viewed_title;
    pthread_mutex_t *mutex;
} ThreadData;

typedef struct {
    char title[128];
    size_t views;
    long memory_usage;
} MostViewedInfo;

typedef struct {
    char *filename;
    char *most_viewed_title;
    uint32_t *most_views;
    size_t *total_memory_usage;
    pthread_mutex_t *mutex;
} MainThreadData;

#endif // STRUCTS_H
