#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <dirent.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>

#define PAGE_SIZE 4096 // Tamaño de la página en bytes

// Estructura para representar una página de datos
typedef struct Page {
    char *data;
    size_t size;
} Page;

typedef struct {
    char title[128];
    long views;
} VideoInfo;

// Función para obtener la lista de archivos .csv en un directorio
char **get_csv_file_list(const char *directory, int *num_files) {
    struct dirent *entry;
    DIR *dp = opendir(directory);
    if (dp == NULL) {
        perror("opendir");
        return NULL;
    }

    char **file_list = NULL;
    *num_files = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_type == DT_REG) {
            const char *ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".csv") == 0) {
                file_list = realloc(file_list, (*num_files + 1) * sizeof(char *));
                if (file_list == NULL) {
                    perror("realloc");
                    closedir(dp);
                    return NULL;
                }
                file_list[*num_files] = malloc(strlen(directory) + strlen(entry->d_name) + 2);
                if (file_list[*num_files] == NULL) {
                    perror("malloc");
                    closedir(dp);
                    return NULL;
                }
                sprintf(file_list[*num_files], "%s/%s", directory, entry->d_name);
                (*num_files)++;
            }
        }
    }
    closedir(dp);
    return file_list;
}

// Función para establecer la afinidad de CPU para un proceso
void set_cpu_affinity(int cpu) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);

    pid_t pid = getpid();
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset) == -1) {
        perror("sched_setaffinity");
        exit(EXIT_FAILURE);
    }
}

// Función para obtener el uso de memoria de un proceso en KB
long get_memory_usage(pid_t pid) {
    char statm_path[256];
    sprintf(statm_path, "/proc/%d/statm", pid);

    FILE *file = fopen(statm_path, "r");
    if (!file) {
        perror("fopen");
        return -1;
    }

    long size;
    if (fscanf(file, "%ld", &size) != 1) {
        perror("fscanf");
        fclose(file);
        return -1;
    }

    fclose(file);
    return size * sysconf(_SC_PAGESIZE) / 1024; // Convert to KB
}


void print_first_line(Page* pages, long* lines_positions, size_t num_lines){
    for (size_t i = 0; i < 1; ++i) {
        long start_pos = lines_positions[i];
        long end_pos = lines_positions[i + 1] - 1;  // Subtract 1 to exclude the newline character

        size_t start_page = start_pos / PAGE_SIZE;
        size_t start_offset = start_pos % PAGE_SIZE;
        size_t end_page = end_pos / PAGE_SIZE;
        size_t end_offset = end_pos % PAGE_SIZE;

        // If the line is contained within the same page
        if (start_page == end_page) {
            fwrite(&pages[start_page].data[start_offset], 1, end_offset - start_offset, stdout);
        } else {
            // Print the part from the start position to the end of the start page
            fwrite(&pages[start_page].data[start_offset], 1, PAGE_SIZE - start_offset, stdout);

            // Print full pages in between if the line spans multiple pages
            for (size_t p = start_page + 1; p < end_page; ++p) {
                fwrite(pages[p].data, 1, PAGE_SIZE, stdout);
            }

            // Print the remaining part in the end page
            fwrite(pages[end_page].data, 1, end_offset, stdout);
        }
        
        // Print a newline to separate lines
        printf("\n");
    }

}

void print_array(long* arr, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%ld ", arr[i]);
    }
    printf("\n");
}

void print_lines(Page* pages, long* lines_positions, size_t num_lines) {
    for (size_t i = 0; i < num_lines - 1; ++i) {
        long start_pos = lines_positions[i];
        long end_pos = lines_positions[i + 1] - 1;  // Subtract 1 to exclude the newline character

        size_t start_page = start_pos / PAGE_SIZE;
        size_t start_offset = start_pos % PAGE_SIZE;
        size_t end_page = end_pos / PAGE_SIZE;
        size_t end_offset = end_pos % PAGE_SIZE;

        // If the line is contained within the same page
        if (start_page == end_page) {
            fwrite(&pages[start_page].data[start_offset], 1, end_offset - start_offset, stdout);
        } else {
            // Print the part from the start position to the end of the start page
            fwrite(&pages[start_page].data[start_offset], 1, PAGE_SIZE - start_offset, stdout);

            // Print full pages in between if the line spans multiple pages
            for (size_t p = start_page + 1; p < end_page; ++p) {
                fwrite(pages[p].data, 1, PAGE_SIZE, stdout);
            }

            // Print the remaining part in the end page
            fwrite(pages[end_page].data, 1, end_offset, stdout);
        }
        
        // Print a newline to separate lines
        printf("\n");
    }
}

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
                
                /*
                if (line_length >= 14000) {  // Skip excessively long lines
                    continue;
                }
                */

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

    // printf("Extracted information for %zu pages.\n", num_pages);
    ////  printf("Extracted information for %zu lines.\n", *num_lines - 1);  // -1 to exclude header

    // Print only valid entries
    /*
    for (size_t i = 1; i < num_lines - 1; ++i) {  // Start from 1 to skip header
        if (video_info_array[i].title[0] != '\0') {
            printf("Line %zu: Title: %s, Views: %ld\n", 
                i, video_info_array[i].title, video_info_array[i].views);
        }
    }
    */
    
    return video_info_array; 
}

// Function to extract the title of the most viewed video


char* extract_most_viewed_video_info(VideoInfo* videos_info, size_t num_lines) {
    long max_views = 0;
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
    videos_info = process_pages_and_extract_data(pages, num_pages, &num_lines, max_lines);
    most_viewed_title = extract_most_viewed_video_info(videos_info, num_lines);
    // print_lines(pages, line_positions, num_lines);
    // print_first_line(pages, line_positions, num_lines);
    // extract_most_viewed_video(filename, pages, line_positions, num_lines);
    // sleep(10);  // Simulate processing time
    long memory_usage_end = get_memory_usage(pid);

    gettimeofday(&file_end, NULL);
    double elapsed_time = ((file_end.tv_sec - file_start.tv_sec) * 1000.0) + ((file_end.tv_usec - file_start.tv_usec) / 1000.0);

    // print_array(line_positions, num_lines);
    // Imprimir la información en una sola línea con formato de tabla, incluyendo el core y número de líneas
    printf("%-10d %-5d %-20s %-10zu %-10zu %-15.6f %-15ld %-15ld %.30s\n", pid, cpu, filename, num_pages, num_lines - 1, elapsed_time, memory_usage_start, memory_usage_end, most_viewed_title);

    if (!is_main_process) {
        write(write_fd, &memory_usage_end, sizeof(long));
        // printf("Reading file %s in process %d on CPU %d\n", filename);
        close(write_fd);
    }

    for (size_t i = 0; i < num_pages; ++i) {
        free(pages[i].data);
    }

    free(pages);
    fclose(file);
    free(line_positions);
    free(videos_info);
    if (!is_main_process) {
        exit(0);
    }
}


int main(int argc, char *argv[]) {
    struct timeval start, first_file_start, end;
    struct tm *local_time;
    char time_str_start[100], time_str_first_file[100], time_str_end[100];

    char *directory = NULL;
    int single = 0;
    int multi = 0;
    int opt;

    // Manejo de los argumentos de línea de comandos
    while ((opt = getopt(argc, argv, "smf:")) != -1) {
        switch (opt) {
            case 's':
                single = 1;
                break;
            case 'm':
                multi = 1;
                break;
            case 'f':
                directory = optarg;
                break;
            default:
                return EXIT_FAILURE;
        }
    }

    if (single && multi) {
        fprintf(stderr, "Error: Cannot use both -s and -m flags simultaneously.\n");
        return EXIT_FAILURE;
    }

    if (directory == NULL) {
        directory = ".";  // Usar el directorio actual si no se especifica uno
    }

    int num_files;
    char **filenames = get_csv_file_list(directory, &num_files);
    if (filenames == NULL) {
        fprintf(stderr, "No .csv files found in directory %s\n", directory);
        return 1;
    }

    pid_t pid = getpid();
    int cpu = sched_getcpu();
    printf("Main process %d is running on CPU %d\n", pid, cpu);

    gettimeofday(&start, NULL);
    local_time = localtime(&start.tv_sec);
    strftime(time_str_start, sizeof(time_str_start), "%H:%M:%S", local_time);


    // Imprimir encabezados de la tabla
    printf("%-10s %-5s %-20s %-10s %-10s %-15s %-15s %-15s %-15s\n", "PID", "Core", "File", "Pages", "Lines", "Time (ms)", "Mem Start (KB)", "Mem End (KB)", "Most viewed");

    // Print the start time of the first file load
    gettimeofday(&first_file_start, NULL);
    local_time = localtime(&first_file_start.tv_sec);
    strftime(time_str_first_file, sizeof(time_str_first_file), "%H:%M:%S", local_time);

    int pipefd[2]; // Pipe file descriptors
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    
    if (single || multi) {
        if (single) {
            set_cpu_affinity(cpu);
        }
        for (int i = 0; i < num_files; i++) {
            pid_t pid = fork();
            if (pid < 0) {
                fprintf(stderr, "Fork failed for file %s: %s\n", filenames[i], strerror(errno));
                return 1;
            } else if (pid == 0) {
                close(pipefd[0]);
                read_file(filenames[i], 0, pipefd[1]);
            }
        }
        
        close(pipefd[1]);
        

        long total_men_usage = 0;
        for (int i = 0; i < num_files; i++) {
            long mem_usage;
            read(pipefd[0], &mem_usage, sizeof(mem_usage));
            total_men_usage += mem_usage;
        }

        for (int i = 0; i < num_files; i++) {
            wait(NULL);
        }
        
        total_men_usage += get_memory_usage(pid);
        long total_men_in_mb = total_men_usage / 1024;
        printf("The total memory usage is: %ld KB (%ld MB)\n", total_men_usage, total_men_in_mb);
    } else {
        for (int i = 0; i < num_files; i++) {
            read_file(filenames[i], 1, -1);
        }
    }

    gettimeofday(&end, NULL);
    local_time = localtime(&end.tv_sec);
    printf("The program starts at %s.%06ld\n", time_str_start, start.tv_usec);
    printf("Start time of the first file load: %s.%06ld\n", time_str_first_file, first_file_start.tv_usec);
    strftime(time_str_end, sizeof(time_str_end), "%H:%M:%S", local_time);
    printf("The program ended at %s.%06ld\n", time_str_end, end.tv_usec);

    double elapsed_time = ((end.tv_sec - start.tv_sec) * 1000.0) + ((end.tv_usec - start.tv_usec) / 1000.0);
    printf("The time used to read %d files is: %f milliseconds.\n", num_files, elapsed_time);

    for (int i = 0; i < num_files; i++) {
        free(filenames[i]);
    }
    free(filenames);

    return 0;
}
