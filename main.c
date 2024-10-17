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
#define RECORDS_PER_THREAD 100 // Número de registros que cada hilo leerá

// Estructura para representar una página de datos
typedef struct Page {
    char *data;
    size_t size;
} Page;

// Estructura para pasar múltiples argumentos a la función de hilo
typedef struct {
    const char *filename;
    long start_offset;
    long end_offset;
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

// Función que será ejecutada por cada hilo, leyendo una porción del archivo
void *read_file_chunk(void *args) {
    ThreadArgs *thread_args = (ThreadArgs *)args;
    const char *filename = thread_args->filename;
    long start_offset = thread_args->start_offset;
    long end_offset = thread_args->end_offset;

    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Could not open file %s: %s\n", filename, strerror(errno));
        return NULL;
    }

    fseek(file, start_offset, SEEK_SET);
    long bytes_to_read = end_offset - start_offset;

    char *buffer = (char *)malloc(bytes_to_read);
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed for reading file chunk\n");
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, bytes_to_read, file);
    // Aquí puedes procesar los datos leídos en buffer (si fuera necesario)

    free(buffer);
    fclose(file);
    return NULL;
}

// Función para manejar el proceso de lectura de archivos por procesos e hilos (modo multi-core)
void *read_file_process(void *filename) {
    const char *file = (const char *)filename;

    FILE *fp = fopen(file, "r");
    if (!fp) {
        fprintf(stderr, "Could not open file %s: %s\n", file, strerror(errno));
        exit(1);
    }

    // Determinar el tamaño del archivo
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Determinar cuántos hilos necesitamos según el tamaño del archivo y la cantidad de registros
    int num_threads = (file_size + (RECORDS_PER_THREAD * PAGE_SIZE) - 1) / (RECORDS_PER_THREAD * PAGE_SIZE);
    pthread_t threads[num_threads];
    ThreadArgs thread_args[num_threads];

    long chunk_size = file_size / num_threads;

    for (int i = 0; i < num_threads; i++) {
        thread_args[i].filename = file;
        thread_args[i].start_offset = i * chunk_size;
        thread_args[i].end_offset = (i == num_threads - 1) ? file_size : (i + 1) * chunk_size;

        if (pthread_create(&threads[i], NULL, read_file_chunk, &thread_args[i]) != 0) {
            fprintf(stderr, "Error creating thread %d for file %s\n", i, file);
            exit(1);
        }
    }

    // Esperar a que todos los hilos terminen
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    fclose(fp);
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
        for (int i = 0; i < num_files; i++) {
            if (pthread_create(&threads[i], NULL, read_file_process, filenames[i]) != 0) {
                fprintf(stderr, "Error creating thread for file %s\n", filenames[i]);
                return 1;
            }
        }

        // Espera a que todos los hilos terminen
        for (int i = 0; i < num_files; i++) {
            pthread_join(threads[i], NULL);
        }

    } else if (multi) {
        // Modo multi-core
        for (int i = 0; i < num_files; i++) {
            pid_t pid = fork();
            if (pid < 0) {
                fprintf(stderr, "Fork failed for file %s: %s\n", filenames[i], strerror(errno));
                return 1;
            } else if (pid == 0) {
                read_file_process(filenames[i]);
                exit(0);
            }
        }

        for (int i = 0; i < num_files; i++) {
            wait(NULL);
        }
    } else {
        // Modo secuencial
        for (int i = 0; i < num_files; i++) {
            read_file_process(filenames[i]);
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
