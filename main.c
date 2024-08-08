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
#include <getopt.h>
#include <libgen.h>
#include <errno.h>
#include <sys/mman.h>  // Para memoria compartida

typedef struct {
    struct timeval start_time;
    struct timeval end_time;
    double duration;
    char filename[256];
} FileTiming;

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
                if (!file_list) {
                    perror("realloc");
                    closedir(dp);
                    return NULL;
                }
                file_list[*num_files] = malloc(strlen(directory) + strlen(entry->d_name) + 2);
                if (!file_list[*num_files]) {
                    perror("malloc");
                    for (int i = 0; i < *num_files; i++) {
                        free(file_list[i]);
                    }
                    free(file_list);
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

void read_file(const char *filename, int is_main_process, FileTiming *timing) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Could not open file %s: %s\n", filename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *file_content = (char *)malloc(file_size + 1);
    if (!file_content) {
        fprintf(stderr, "Memory allocation failed for file %s\n", filename);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    gettimeofday(&timing->start_time, NULL); // Tiempo de inicio

    size_t read_size = fread(file_content, 1, file_size, file);
    if (read_size != file_size) {
        fprintf(stderr, "File read failed for file %s\n", filename);
        free(file_content);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    gettimeofday(&timing->end_time, NULL); // Tiempo de fin

    timing->duration = (timing->end_time.tv_sec - timing->start_time.tv_sec) +
                       (timing->end_time.tv_usec - timing->start_time.tv_usec) / 1e6;

    snprintf(timing->filename, sizeof(timing->filename), "%s", filename);

    free(file_content);
    fclose(file);

    if (!is_main_process) {
        exit(0);  // El proceso hijo termina aquí
    }
}

int main(int argc, char *argv[]) {
    struct timeval start, end;
    struct tm *local_time;
    char time_str_start[100], time_str_end[100];

    char *directory = NULL;
    int single = 0;
    int multi = 0;
    int opt;

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
                fprintf(stderr, "Usage: %s [-s | -m] [-f <folder>]\n", argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (single && multi) {
        fprintf(stderr, "Error: Cannot use both -s and -m flags simultaneously.\n");
        return EXIT_FAILURE;
    }

    if (!directory) {
        char exe_path[1024];
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len == -1) {
            perror("readlink");
            return EXIT_FAILURE;
        }
        exe_path[len] = '\0';
        directory = dirname(exe_path);
        printf("No directory specified. Using current executable directory: %s\n", directory);
    }

    int num_files;
    char **filenames = get_csv_file_list(directory, &num_files);
    if (filenames == NULL) {
        fprintf(stderr, "No .csv files found in directory %s\n", directory);
        return 1;
    }

    // Crear un área de memoria compartida para FileTiming
    FileTiming *timings = mmap(NULL, num_files * sizeof(FileTiming), PROT_READ | PROT_WRITE,
                               MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (timings == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    pid_t pid = getpid();
    int cpu = sched_getcpu();
    printf("Process %d is running on CPU %d\n", pid, cpu);

    gettimeofday(&start, NULL);
    local_time = localtime(&start.tv_sec);
    strftime(time_str_start, sizeof(time_str_start), "%H:%M:%S", local_time);
    printf("The program starts at %s.%06ld\n", time_str_start, start.tv_usec);

    if (single || multi) {
        if (single) {
            set_cpu_affinity(cpu);
        }

        for (int i = 0; i < num_files; i++) {
            pid_t pid = fork();
            if (pid < 0) {
                fprintf(stderr, "Fork failed for file %s\n", filenames[i]);
                return 1;
            } else if (pid == 0) {
                read_file(filenames[i], 0, &timings[i]);
            }
        }

        for (int i = 0; i < num_files; i++) {
            wait(NULL);
        }
    } else {
        for (int i = 0; i < num_files; i++) {
            read_file(filenames[i], 1, &timings[i]);
        }
    }

    gettimeofday(&end, NULL);
    local_time = localtime(&end.tv_sec);
    strftime(time_str_end, sizeof(time_str_end), "%H:%M:%S", local_time);
    printf("The program ended at %s.%06ld\n", time_str_end, end.tv_usec);

    double elapsed_time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
    printf("The total time used to read %d files is: %f seconds.\n", num_files, elapsed_time);

    // Mostrar la tabla resumen
    printf("\nFilename\t\t\tStart Time\t\tEnd Time\t\tDuration (s)\n");
    printf("-------------------------------------------------------------------------------\n");
    for (int i = 0; i < num_files; i++) {
        struct tm *start_tm = localtime(&timings[i].start_time.tv_sec);
        struct tm *end_tm = localtime(&timings[i].end_time.tv_sec);
        char start_str[64], end_str[64];
        strftime(start_str, sizeof(start_str), "%H:%M:%S", start_tm);
        strftime(end_str, sizeof(end_str), "%H:%M:%S", end_tm);
        printf("%s\t%s.%06ld\t%s.%06ld\t%f\n", timings[i].filename, start_str, timings[i].start_time.tv_usec, end_str, timings[i].end_time.tv_usec, timings[i].duration);
    }

    // Liberar memoria compartida
    munmap(timings, num_files * sizeof(FileTiming));

    // Liberar otros recursos
    for (int i = 0; i < num_files; i++) {
        free(filenames[i]);
    }
    free(filenames);

    return 0;
}
