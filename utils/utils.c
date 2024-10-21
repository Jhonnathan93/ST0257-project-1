#define _GNU_SOURCE


#include "utils.h"
#include "constants.h"
#include "structs.h"

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


long get_memory_usage(pid_t pid) {
    char statm_path[128];
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

void print_array(long* arr, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%ld ", arr[i]);
    }
    printf("\n");
}
