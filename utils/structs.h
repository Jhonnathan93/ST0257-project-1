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

#endif // STRUCTS_H
