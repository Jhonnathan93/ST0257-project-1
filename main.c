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
                file_list[*num_files] = malloc(strlen(directory) + strlen(entry->d_name) + 2);
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

void read_file(const char *filename, int is_main_process) {
    for (int i = 0; i < 10; i++) {
        FILE *file = fopen(filename, "r");
        if (!file) {
            fprintf(stderr, "Could not open file %s\n", filename);
            exit(1);
        }

        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        char *file_content = (char *)malloc(file_size + 1);
        if (!file_content) {
            fprintf(stderr, "Memory allocation failed for file %s\n", filename);
            // free(file_content);
            fclose(file);
            exit(1);
        }

    // file_content[file_size] = '\0';
    // free(file_content);
    // fclose(file);


    // start = clock();

    // Read the entire file into the allocated buffer
        size_t read_size = fread(file_content, 1, file_size, file);
        if (read_size != file_size) {
            fprintf(stderr, "File read failed for file %s\n", filename);
            free(file_content);
            fclose(file);
            exit(1);
        }

    file_content[file_size] = '\0';
    file_content[file_size] = '\0';
    // end = clock();

    // cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    // Print the file size
    // printf("Read the file, The number of chars in file %s is: %ld and time used is: %f seconds.\n",filename, file_size, cpu_time_used);

    // printf("Contenido del archivo %s:\n%s\n", filename, file_content);

    // Clean up
        file_content[file_size] = '\0';
    // end = clock();

    // cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    // Print the file size
    // printf("Read the file, The number of chars in file %s is: %ld and time used is: %f seconds.\n",filename, file_size, cpu_time_used);

    // printf("Contenido del archivo %s:\n%s\n", filename, file_content);

    // Clean up
        free(file_content);
        fclose(file);
    }
    if (!is_main_process) {
        exit(0);
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
                fprintf(stderr, "Usage: %s [-s | -m] -f <folder>\n", argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (single && multi) {
        fprintf(stderr, "Error: Cannot use both -s and -m flags simultaneously.\n");
        return EXIT_FAILURE;
    }

    if (!directory) {
        fprintf(stderr, "Error: Directory not specified.\n");
        return EXIT_FAILURE;
    }

    int num_files;
    char **filenames = get_csv_file_list(directory, &num_files);
    if (filenames == NULL) {
        fprintf(stderr, "No .csv files found in directory %s\n", directory);
        return 1;
    }
    // Get the process ID and the CPU it is running on    
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
                read_file(filenames[i], 0);
            }
        }

        for (int i = 0; i < num_files; i++) {
            wait(NULL);
        }
    } else {
        for (int i = 0; i < num_files; i++) {
            read_file(filenames[i], 1);
        }
    }

    gettimeofday(&end, NULL);
    local_time = localtime(&end.tv_sec);
    strftime(time_str_end, sizeof(time_str_end), "%H:%M:%S", local_time);
    printf("The program ended at %s.%06ld\n", time_str_end, end.tv_usec);

    double elapsed_time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
    printf("The time used to read %d files is: %f seconds.\n", num_files, elapsed_time);

    for (int i = 0; i < num_files; i++) {
        free(filenames[i]);
    }
    free(filenames);

    return 0;
}
