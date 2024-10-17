
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 1024

// Function to get the column index from the CSV header
int get_column_index(char *header, const char *column_name) {
    char *token = strtok(header, ",");
    int index = 0;
    while (token) {
        if (strcmp(token, column_name) == 0) {
            return index;
        }
        token = strtok(NULL, ",");
        index++;
    }
    return -1; // Column not found
}

// Function to find the video with the most views
void find_most_popular_video(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Could not open file %s\n", filename);
        return;
    }

    char line[MAX_LINE_LENGTH];
    char *headers = fgets(line, MAX_LINE_LENGTH, file); // Read the header line
    if (!headers) {
        fprintf(stderr, "Failed to read the header from file %s\n", filename);
        fclose(file);
        return;
    }

    // Find the index for the columns we're interested in
    int views_index = get_column_index(headers, "views");
    int title_index = get_column_index(headers, "title");
    int video_id_index = get_column_index(headers, "video_id");

    if (views_index == -1 || title_index == -1 || video_id_index == -1) {
        fprintf(stderr, "Required columns not found in CSV header.\n");
        fclose(file);
        return;
    }

    long max_views = 0;
    char most_popular_title[MAX_LINE_LENGTH] = {0};
    char most_popular_video_id[MAX_LINE_LENGTH] = {0};

    // Read each line of the CSV
    while (fgets(line, MAX_LINE_LENGTH, file)) {
        char *token;
        int current_column = 0;
        long current_views = 0;
        char current_title[MAX_LINE_LENGTH] = {0};
        char current_video_id[MAX_LINE_LENGTH] = {0};

        // Tokenize the line by commas
        token = strtok(line, ",");
        while (token) {
            // We are now iterating through columns, match the index
            if (current_column == views_index) {
                current_views = strtol(token, NULL, 10); // Convert views to a long integer
            } else if (current_column == title_index) {
                strncpy(current_title, token, MAX_LINE_LENGTH - 1);
            } else if (current_column == video_id_index) {
                strncpy(current_video_id, token, MAX_LINE_LENGTH - 1);
            }

            token = strtok(NULL, ",");
            current_column++;
        }

        // Check if this video has more views than the current max
        if (current_views > max_views) {
            max_views = current_views;
            strncpy(most_popular_title, current_title, MAX_LINE_LENGTH - 1);
            strncpy(most_popular_video_id, current_video_id, MAX_LINE_LENGTH - 1);
        }
    }

    fclose(file);

    // Output the most popular video
    printf("Most popular video:\n");
    printf("Video ID: %s\n", most_popular_video_id);
    printf("Title: %s\n", most_popular_title);
    printf("Views: %ld\n", max_views);
}

int main() {
    const char *csv_file = "files/CAvideos.csv";
    find_most_popular_video(csv_file);
    return 0;
}
