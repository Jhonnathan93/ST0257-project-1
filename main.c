#define _GNU_SOURCE

#include "libs.h"

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

    if (single || multi) {
        
        int pipefd[2]; // Pipe file descriptors
        if (pipe(pipefd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

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

        printf("The total memory usage is: %ld KB\n", get_memory_usage(pid));
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
