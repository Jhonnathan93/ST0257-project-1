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
#include <pthread.h> // Biblioteca para hilos (threads)

#define PAGE_SIZE 4096 // Tamaño de la página en bytes

// Estructura para representar una página de datos
typedef struct Page {
    char *data;
    size_t size;
} Page;

// Estructura para pasar múltiples argumentos a la función de hilo
typedef struct {
    const char *filename;
    int is_main_process;
} ThreadArgs;

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

// Función para leer un archivo en páginas
void *read_file_thread(void *args) {
    ThreadArgs *thread_args = (ThreadArgs *)args;
    const char *filename = thread_args->filename;

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
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    size_t num_pages = (file_size + PAGE_SIZE - 1) / PAGE_SIZE;

    Page *pages = (Page *)malloc(num_pages * sizeof(Page));
    if (!pages) {
        fprintf(stderr, "Memory allocation failed for file %s\n", filename);
        fclose(file);
        return NULL;
    }

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
            return NULL;
        }
    }

    size_t read_size;
    for (size_t i = 0; i < num_pages; ++i) {
        read_size = fread(pages[i].data, 1, PAGE_SIZE, file);
        pages[i].size = read_size;
        if (read_size < PAGE_SIZE && ferror(file)) {
            fprintf(stderr, "File read failed for file %s: %s\n", filename, strerror(errno));
            for (size_t j = 0; j < num_pages; ++j) {
                free(pages[j].data);
            }
            free(pages);
            fclose(file);
            return NULL;
        }
    }

    long memory_usage_end = get_memory_usage(pid);

    gettimeofday(&file_end, NULL);
    double elapsed_time = ((file_end.tv_sec - file_start.tv_sec) * 1000.0) + ((file_end.tv_usec - file_start.tv_usec) / 1000.0);

    // Imprimir la información en una sola línea con formato de tabla, incluyendo el core
    printf("%-10d %-5d %-20s %-10zu %-20.6f %-15ld %-15ld\n", pid, cpu, filename, num_pages, elapsed_time, memory_usage_start, memory_usage_end);

    for (size_t i = 0; i < num_pages; ++i) {
        free(pages[i].data);
    }

    free(pages);
    fclose(file);

    return NULL;
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
    printf("%-10s %-5s %-20s %-10s %-20s %-15s %-15s\n", "PID", "Core", "File", "Pages", "Time (ms)", "Mem Start (KB)", "Mem End (KB)");

    // Print the start time of the first file load
    gettimeofday(&first_file_start, NULL);
    local_time = localtime(&first_file_start.tv_sec);
    strftime(time_str_first_file, sizeof(time_str_first_file), "%H:%M:%S", local_time);

    if (single) {
        set_cpu_affinity(cpu); // Establece afinidad de CPU para el proceso principal

        // Crea hilos para cada archivo CSV
        pthread_t threads[num_files];
        ThreadArgs thread_args[num_files];

        for (int i = 0; i < num_files; i++) {
            thread_args[i].filename = filenames[i];
            thread_args[i].is_main_process = 1;
            if (pthread_create(&threads[i], NULL, read_file_thread, &thread_args[i]) != 0) {
                fprintf(stderr, "Error creating thread for file %s\n", filenames[i]);
                return 1;
            }
        }

        // Espera a que todos los hilos terminen
        for (int i = 0; i < num_files; i++) {
            pthread_join(threads[i], NULL);
        }

    } else if (multi) {
        // Implementación para multi-core mode (fork)
        for (int i = 0; i < num_files; i++) {
            pid_t pid = fork();
            if (pid < 0) {
                fprintf(stderr, "Fork failed for file %s: %s\n", filenames[i], strerror(errno));
                return 1;
            } else if (pid == 0) {
                read_file_thread(&(ThreadArgs){filenames[i], 0});
            }
        }

        for (int i = 0; i < num_files; i++) {
            wait(NULL);
        }
    } else {
        // Default mode (single process)
        for (int i = 0; i < num_files; i++) {
            read_file_thread(&(ThreadArgs){filenames[i], 1});
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
