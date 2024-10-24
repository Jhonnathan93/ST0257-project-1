#define _GNU_SOURCE

#include "reader.h"

void read_pages(const char* filename, Page** pages, size_t* num_pages) {

    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Could not open file %s: %s\n", filename, strerror(errno));
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    (* num_pages) = (file_size + PAGE_SIZE - 1) / PAGE_SIZE;

    *pages = (Page *)malloc((* num_pages) * sizeof(Page));
    if (!*pages) {
        fprintf(stderr, "Memory allocation failed for file %s\n", filename);
        fclose(file);
        exit(1);
    }

    size_t num_lines = 0;
    size_t max_lines = 1000;  // Initial capacity for line positions array
    long *line_positions = (long *)malloc(max_lines * sizeof(long));

    if (!line_positions) {
        fprintf(stderr, "Memory allocation failed for line positions\n");
        free(*pages);
        fclose(file);
        exit(1);
    }

    line_positions[num_lines++] = 0;  // First line starts at the beginning of the file


    size_t read_size;
    for (size_t i = 0; i < (* num_pages); ++i) {
        (* pages)[i].data = (char *)malloc(PAGE_SIZE);
        (* pages)[i].size = 0;
    
    if (!(* pages)[i].data) {
            fprintf(stderr, "Memory allocation failed for page %zu\n", i);
            for (size_t j = 0; j < i; ++j) {
                free((* pages)[j].data);
            }
            free((*pages));
            fclose(file);
            exit(1);
        }

        // Read data and count lines
        read_size = fread((* pages)[i].data, 1, PAGE_SIZE, file);
        (* pages)[i].size = read_size;
        if (read_size < PAGE_SIZE && ferror(file)) {
            fprintf(stderr, "File read failed for file %s: %s\n", filename, strerror(errno));
            for (size_t j = 0; j < (* num_pages); ++j) {
                free((* pages)[j].data);
            }
            free((* pages));
            fclose(file);
            exit(1);
        }
    }

}

void read_file(const char *filename, int is_main_process, int write_fd) {
    pid_t pid = getpid();
    int cpu = sched_getcpu();  // Obtener el core donde se está ejecutando el proceso
    long memory_usage_start = get_memory_usage(pid);

    if (memory_usage_start == -1) {
        fprintf(stderr, "Could not get memory usage for process %d\n", pid);
    }
    
    
    struct timespec file_start, file_end;
    clock_gettime(CLOCK_MONOTONIC, &file_start);

    Page* pages;
    size_t num_pages;

    read_pages(filename, &pages, &num_pages);

    VideoInfo* videos_info;
    char* most_viewed_title;

    size_t max_views;
    size_t max_lines = 1000;
    size_t num_lines;
    
    videos_info = process_pages_and_extract_data(pages, num_pages, &num_lines, max_lines);
     
    divide_and_process_chunks(videos_info, &most_viewed_title, &max_views, num_lines);

    clock_gettime(CLOCK_MONOTONIC, &file_end);
    double elapsed_time = (file_end.tv_sec - file_start.tv_sec) * 1000.0 + (file_end.tv_nsec - file_start.tv_nsec) / 1e6;

    long memory_usage_end = get_memory_usage(pid);
    
    // printf("num_pages = %d\n", num_pages);
    // printf("num_lines = %d\n", num_lines);
    // printf("most_viewed_title = %s", most_viewed_title);
    printf("%-10d", pid);
    printf("%-5d", cpu);
    printf("%-20s", filename);
    printf("%-10zu", num_pages);
    printf("%-10zu", num_lines - 1);
    printf("%-15f", elapsed_time);
    printf("%-15ld", memory_usage_start);
    printf("%-15ld", memory_usage_end);
    printf("%.20s\n", most_viewed_title);
  
    printf("%-10d %-5d %-20s %-10zu %-10zu %-15.6f %-15ld %-15ld %.20s\n", pid, cpu, filename, num_pages, num_lines - 1, elapsed_time, memory_usage_start, memory_usage_end, most_viewed_title);

    if (!is_main_process) {
        write(write_fd, &memory_usage_end, sizeof(long));
        // printf("Reading file %s in process %d on CPU %d\n", filename);
        close(write_fd);
    }
    
    for (size_t i = 0; i < num_pages; ++i) {
        free(pages[i].data);
    }

    free(pages);


    free(videos_info);

    if (!is_main_process)
        exit(0);
}
