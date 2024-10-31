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
    printf("%-10s %-5s %-20s %-10s %-10s %-15s %-15s %-15s %-15s %-15s\n", "PID", "Core", "File", "Pages", "Lines", "Time (ms)", "Mem Start (KB)", "Mem End (KB)", "Most viewed", "Max views");

    // Print the start time of the first file load
    gettimeofday(&first_file_start, NULL);
    local_time = localtime(&first_file_start.tv_sec);
    strftime(time_str_first_file, sizeof(time_str_first_file), "%H:%M:%S", local_time); 

    if (multi) {

        int pipefd[2]; // Pipe file descriptors
        if (pipe(pipefd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
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
        MostViewedInfo overall_most_viewed;
        overall_most_viewed.views = 0; // Initialize to zero
        overall_most_viewed.memory_usage = 0;    

        for (int i = 0; i < num_files; i++) {
            MostViewedInfo info;

            read(pipefd[0], &info, sizeof(MostViewedInfo));

            total_men_usage += info.memory_usage;
            if (info.views > overall_most_viewed.views) {
                overall_most_viewed.views = info.views;
                strncpy(overall_most_viewed.title, info.title, sizeof(overall_most_viewed.title));
            }
        }

        for (int i = 0; i < num_files; i++) {
            wait(NULL);
        }

        printf("Overall most viewed title is: %s with %zu views.\n",
               overall_most_viewed.title, overall_most_viewed.views);

        total_men_usage += get_memory_usage(pid);
        long total_men_in_mb = total_men_usage / 1024;
        printf("The total memory usage is: %ld KB (%ld MB)\n", total_men_usage, total_men_in_mb);
    } else if (single) {
        pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
        char *most_viewed_title = (char *)malloc(128 * sizeof(char));
        uint32_t most_views = 0;
        pthread_t threads[num_files];
        size_t total_memory_usage = 0;
        MainThreadData threads_data[num_files];
        
        if (!most_viewed_title) {
            fprintf(stderr, "Memory allocation for most_viewed_title failed.\n");
            return EXIT_FAILURE;
        }

        pthread_attr_t attr;
        pthread_attr_init(&attr);

        // Set the stack size attribute
        if (pthread_attr_setstacksize(&attr, STACK_SIZE) != 0) {
            perror("Failed to set stack size");
            return EXIT_FAILURE;
        }

        for (int i = 0; i < num_files; i++) {
            threads_data[i].filename = filenames[i];
            threads_data[i].most_viewed_title = most_viewed_title;
            threads_data[i].most_views = &most_views;
            threads_data[i].total_memory_usage = &total_memory_usage;
            threads_data[i].mutex = &mutex;
            pthread_create(&threads[i], &attr, read_file_thread, &threads_data[i]);
        }

        for (int i = 0; i < num_files; i++) {
            pthread_join(threads[i], NULL);
        }
 
        printf("Overall most viewed title is: %s with %zu views.\n", most_viewed_title, most_views);
        printf("The total memory usage is: %ld KB (%ld MB)\n", total_memory_usage, total_memory_usage / 1024);
    } else {
        for (int i = 0; i < num_files; i++) {
            read_file(filenames[i], 1, -1);
        }

        long total_men_usage = get_memory_usage(pid);
        long total_men_in_mb = total_men_usage / 1024;
        printf("The total memory usage is: %ld KB (%ld MB) \n", total_men_usage, total_men_in_mb);
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
