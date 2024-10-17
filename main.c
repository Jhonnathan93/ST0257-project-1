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

// Estructura para pasar resultados al proceso principal
typedef struct {
    pid_t pid;
    int cpu;
    char filename[256];
    size_t num_pages;
    double time_ms;
    long mem_start;
    long mem_end;
} Result;

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
void *read_file_process(void *filename, int result_pipe) {
    const char *file = (const char *)filename;

    pid_t pid = getpid();
    int cpu = sched_getcpu();

    // Obtener el uso de memoria inicial
    long mem_start = get_memory_usage(pid);

    FILE *fp = fopen(file, "r");
    if (!fp) {
        fprintf(stderr, "Could not open file %s: %s\n", file, strerror(errno));
        exit(1);
    }

    // Obtener el tamaño del archivo
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Dividir el archivo entre hilos
    int num_threads = (file_size + (RECORDS_PER_THREAD * PAGE_SIZE) - 1) / (RECORDS_PER_THREAD * PAGE_SIZE);
    pthread_t threads[num_threads];
    ThreadArgs thread_args[num_threads];
    long chunk_size = file_size / num_threads;

    // Marcar el tiempo de inicio
    struct timeval file_start, file_end;
    gettimeofday(&file_start, NULL);

    for (int i = 0; i < num_threads; i++) {
        thread_args[i].filename = file;
        thread_args[i].start_offset = i * chunk_size;
        thread_args[i].end_offset = (i == num_threads - 1) ? file_size : (i + 1) * chunk_size;

        if (pthread_create(&threads[i], NULL, read_file_chunk, &thread_args[i]) != 0) {
            fprintf(stderr, "Error creating thread %d for file %s\n", i, file);
            exit(1);
        }
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    fclose(fp);

    // Marcar el tiempo de fin
    gettimeofday(&file_end, NULL);
    double time_ms = ((file_end.tv_sec - file_start.tv_sec) * 1000.0) + ((file_end.tv_usec - file_start.tv_usec) / 1000.0);

    // Obtener el uso de memoria final
    long mem_end = get_memory_usage(pid);

    // Pasar el resultado al proceso principal a través de una tubería
    Result result;
    result.pid = pid;
    result.cpu = cpu;
    strncpy(result.filename, file, sizeof(result.filename));
    result.num_pages = num_threads;
    result.time_ms = time_ms;
    result.mem_start = mem_start;
    result.mem_end = mem_end;

    write(result_pipe, &result, sizeof(Result));

    return NULL;
}

// Función que recopila los resultados y los imprime en la tabla
void collect_results(int result_pipe, int num_files) {
    printf("%-10s %-5s %-20s %-10s %-20s %-15s %-15s\n", "PID", "Core", "File", "Pages", "Time (ms)", "Mem Start (KB)", "Mem End (KB)");
    
    for (int i = 0; i < num_files; i++) {
        Result result;
        read(result_pipe, &result, sizeof(Result));

        printf("%-10d %-5d %-20s %-10zu %-20.6f %-15ld %-15ld\n", result.pid, result.cpu, result.filename, result.num_pages, result.time_ms, result.mem_start, result.mem_end);
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

    // Crear una tubería para enviar los resultados desde los procesos hijos
    int result_pipe[2];
    if (pipe(result_pipe) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    if (multi) {
        for (int i = 0; i < num_files; i++) {
            pid_t pid = fork();
            if (pid < 0) {
                fprintf(stderr, "Fork failed for file %s: %s\n", filenames[i], strerror(errno));
                return 1;
            } else if (pid == 0) {
                // Procesos hijos (modo multi-core)
                close(result_pipe[0]);  // Cerramos el lado de lectura de la tubería en los procesos hijos
                read_file_process(filenames[i], result_pipe[1]);
                exit(0);
            }
        }

        close(result_pipe[1]);  // Cerramos el lado de escritura en el proceso padre
        collect_results(result_pipe[0], num_files);  // Recopilar los resultados en el proceso padre
        close(result_pipe[0]);  // Cerrar la tubería de lectura al finalizar

    } else if (single) {
        // Implementar el modo single-core con hilos
    }

    gettimeofday(&end, NULL);
    printf("The program started at %s\n", time_str_start);
    strftime(time_str_end, sizeof(time_str_end), "%H:%M:%S", local_time);
    printf("The program ended at %s\n", time_str_end);

    return 0;
}
