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
#define RECORDS_PER_THREAD 100 // Registros que leerá cada hilo

// Estructura para representar una página de datos
typedef struct Page {
    char *data;
    size_t size;
} Page;

// Estructura para pasar múltiples argumentos a la función de hilo
typedef struct {
    const char *filename;
    size_t start_offset;
    size_t end_offset;
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

// Función que será ejecutada por cada hilo para leer parte del archivo
void *read_chunk(void *args) {
    ThreadArgs *thread_args = (ThreadArgs *)args;
    const char *filename = thread_args->filename;
    size_t start_offset = thread_args->start_offset;
    size_t end_offset = thread_args->end_offset;

    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Could not open file %s: %s\n", filename, strerror(errno));
        return NULL;
    }

    // Mover el puntero de archivo a la posición de inicio
    fseek(file, start_offset, SEEK_SET);

    // Leer el fragmento del archivo hasta el end_offset
    size_t size_to_read = end_offset - start_offset;
    char *buffer = (char *)malloc(size_to_read);
    fread(buffer, 1, size_to_read, file);

    // Simulamos el procesamiento del fragmento leído
    printf("Thread reading chunk from offset %zu to %zu in file %s\n", start_offset, end_offset, filename);

    free(buffer);
    fclose(file);
    return NULL;
}

// Función para leer un archivo utilizando hilos que procesan diferentes partes del archivo
void read_file_with_threads(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Could not open file %s: %s\n", filename, strerror(errno));
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    fclose(file);

    // Calcular cuántos hilos son necesarios para procesar el archivo completo
    int num_threads = (file_size + RECORDS_PER_THREAD - 1) / RECORDS_PER_THREAD;

    pthread_t threads[num_threads];
    ThreadArgs thread_args[num_threads];

    // Dividir el archivo en partes para que cada hilo lea un fragmento
    for (int i = 0; i < num_threads; i++) {
        size_t start_offset = i * RECORDS_PER_THREAD;
        size_t end_offset = (i + 1) * RECORDS_PER_THREAD;
        if (end_offset > file_size) {
            end_offset = file_size;
        }

        thread_args[i].filename = filename;
        thread_args[i].start_offset = start_offset;
        thread_args[i].end_offset = end_offset;

        if (pthread_create(&threads[i], NULL, read_chunk, &thread_args[i]) != 0) {
            fprintf(stderr, "Error creating thread for file %s\n", filename);
            exit(1);
        }
    }

    // Esperar a que todos los hilos terminen
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
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
    printf("%-10s %-5s %-20s %-10s %-20s %-15s %-15s\n", "PID", "Core", "File", "Pages", "Time (ms)", "Mem Start (KB)", "Mem End (KB)");

    // Print the start time of the first file load
    gettimeofday(&first_file_start, NULL);
    local_time = localtime(&first_file_start.tv_sec);
    strftime(time_str_first_file, sizeof(time_str_first_file), "%H:%M:%S", local_time);

    if (multi) {
        // Modo multi-core: crear un proceso por cada archivo y leer el archivo en hilos
        for (int i = 0; i < num_files; i++) {
            pid_t pid = fork();
            if (pid < 0) {
                fprintf(stderr, "Fork failed for file %s: %s\n", filenames[i], strerror(errno));
                return 1;
            } else if (pid == 0) {
                // Proceso hijo: leer archivo usando hilos
                read_file_with_threads(filenames[i]);
                exit(0); // Termina el proceso hijo
            }
        }

        // Esperar a que todos los procesos hijos terminen
        for (int i = 0; i < num_files; i++) {
            wait(NULL);
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
