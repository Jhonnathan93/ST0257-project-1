#define _GNU_SOURCE

#include "reader.h"

void read_file(const char *filename, int is_main_process, int write_fd) {
    pid_t pid = getpid();
    int cpu = sched_getcpu();  // Obtener el core donde se está ejecutando el proceso
    
    long memory_usage_start = get_memory_usage(pid);
    if (memory_usage_start == -1) {
        fprintf(stderr, "Could not get memory usage for process %d\n", pid);
    }

    struct timeval file_start, file_end;
    gettimeofday(&file_start, NULL);

    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Could not open file %s: %s\n", filename, strerror(errno));
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    size_t num_pages = (file_size + PAGE_SIZE - 1) / PAGE_SIZE;

    Page *pages = (Page *)malloc(num_pages * sizeof(Page));
    if (!pages) {
        fprintf(stderr, "Memory allocation failed for file %s\n", filename);
        fclose(file);
        exit(1);
    }

    // Initialize the line count
    
    size_t num_lines = 0;
    size_t max_lines = 1000;  // Initial capacity for line positions array
    long *line_positions = (long *)malloc(max_lines * sizeof(long));

    if (!line_positions) {
        fprintf(stderr, "Memory allocation failed for line positions\n");
        exit(1);
    }

    line_positions[num_lines++] = 0;  // First line starts at the beginning of the file


    size_t read_size;
    for (size_t i = 0; i < num_pages; ++i) {
        pages[i].data = (char *)malloc(PAGE_SIZE);
        pages[i].size = 0;
    
    if (!pages[i].data) {
            fprintf(stderr, "Memory allocation failed for page %zu\n", i);
            for (size_t j = 0; j < i; ++j) {
                free(pages[j].data);
            }
            free(pages);
            fclose(file);
            exit(1);
        }

        // Read data and count lines
        read_size = fread(pages[i].data, 1, PAGE_SIZE, file);
        pages[i].size = read_size;
        if (read_size < PAGE_SIZE && ferror(file)) {
            fprintf(stderr, "File read failed for file %s: %s\n", filename, strerror(errno));
            for (size_t j = 0; j < num_pages; ++j) {
                free(pages[j].data);
            }
            free(pages);
            fclose(file);
            exit(1);
        }

        // Count the number of newlines in the page
        // for (size_t j = 0; j < pages[i].size; ++j) {
        //    if (pages[i].data[j] == '\n') {
        //        if (num_lines >= max_lines) {
        //            // Increase max_lines and reallocate
        //            max_lines *= 2;
        //            line_positions = (long *)realloc(line_positions, max_lines * sizeof(long));
        //            if (!line_positions) {
        //                fprintf(stderr, "Reallocation failed for line positions\n");
        //                exit(1);
        //            }
        //        }
        //        line_positions[num_lines++] = (i * PAGE_SIZE) + j + 1;  // Next line starts after '\n'
        //    }
        // }
    }

    VideoInfo* videos_info;
    char* most_viewed_title;
    size_t max_views;
    videos_info = process_pages_and_extract_data(pages, num_pages, &num_lines, max_lines);
     
    divide_and_process_chunks(videos_info, &most_viewed_title, &max_views, num_lines);
    // most_viewed_title = extract_most_viewed_video_info(videos_info, num_lines);
    // print_lines(pages, line_positions, num_lines);
    // print_first_line(pages, line_positions, num_lines);
    // extract_most_viewed_video(filename, pages, line_positions, num_lines);
    // sleep(10);  // Simulate processing time

    gettimeofday(&file_end, NULL);
    double elapsed_time = ((file_end.tv_sec - file_start.tv_sec) * 1000.0) + ((file_end.tv_usec - file_start.tv_usec) / 1000.0);
    
    long memory_usage_end = get_memory_usage(pid);
    printf("Memory usage for process %d: %ld KB\n", pid, memory_usage_end);
    // print_array(line_positions, num_lines);
    // Imprimir la información en una sola línea con formato de tabla, incluyendo el core y número de líneas
    printf("%-10d %-5d %-20s %-10zu %-10zu %-15.6f %-15ld %-15ld %.15s\n", pid, cpu, filename, num_pages, num_lines - 1, elapsed_time, memory_usage_start, memory_usage_end, most_viewed_title);

    if (!is_main_process) {
        // write(write_fd, &memory_usage_end, sizeof(long));
        // printf("Reading file %s in process %d on CPU %d\n", filename);
    }
    
    for (size_t i = 0; i < num_pages; ++i) {
        free(pages[i].data);
    }
    close(write_fd);
    fclose(file);
    free(pages);
    free(line_positions);
    free(videos_info);
    if (!is_main_process) {
        exit(0);
    }
}
