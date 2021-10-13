// CS-3013 Assignment 4
// By Sirut Buasai
// sbuasai2@wpi.edu

#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>

using namespace std;

#define MAX_BYTE_READ 1024
#define MAX_QUEUE 32
#define MAX_THREADS 15

/* ------------------------------------------- Global Variables ------------------------------------------- */
bool mutex;
pthread_t thread_arr[MAX_THREADS] = {0};
pthread_t thread_queue[MAX_QUEUE];
char file_arr[MAX_THREADS][64];
int file_idx = 0;
int q_front = 0;
int q_rear = MAX_QUEUE - 1;
int q_size = 0;
int bad_files = 0;
int directories = 0;
int regular_files = 0;
int special_files = 0;
int regular_files_size = 0;
int text_files = 0;
int text_files_size = 0;

/* ------------------------------------------- Queue ------------------------------------------- */

bool is_empt() {
    return (q_size == 0);
}

bool is_full() {
    return (q_size == MAX_QUEUE);
}

void enqueue(pthread_t *ptr_thrID) {
    if(is_full()) {
        cerr << "Queue is full" << endl;
        cerr.flush();
    }
    else {
        q_rear = (q_rear + 1) % MAX_QUEUE;
        thread_queue[q_rear] = (*ptr_thrID);
        q_size++;
    }
}

void dequeue(pthread_t *ptr_thrID) {
    if(is_empt()) {
        cerr << "Queue is empty" << endl;
        cerr.flush();
    }
    else {
        *ptr_thrID = thread_queue[q_front];
        q_size--;
        q_front = (q_front + 1) % MAX_QUEUE;
    }
}

/* ------------------------------------------- Functions ------------------------------------------- */

void print_help() {
    cerr << "./proj4 only accepts either stdin or 2 arguments." << endl;
    cerr << "Program also allows pipe as input." << endl;
    cerr << "Usage: ./proj4 [optional: \"thread\" <number>]" << endl;
    cerr << "Example: ./proj4" << endl;
    cerr << "         ls -1d /dev/* | ./proj4" << endl;
    cerr << "         ./proj4 thread 7" << endl;
    cerr << "         ls -1d /dev/* | ./proj4 thread 7" << endl;
    cerr.flush();
    exit(1);
}

void print_results() {
    cout << "Bad Files " << bad_files << endl;
    cout << "Directories: " << directories << endl;
    cout << "Regular Files: " << regular_files << endl;
    cout << "Special Files: " << special_files << endl;
    cout << "Regular Files Bytes: " << regular_files_size << endl;
    cout << "Text Files: " << text_files << endl;
    cout << "Text Files Bytes: " << text_files_size << endl;
}

bool is_integer(char *str) {
    for (int i = 0; i < strlen(str); i++) {
        if (!isdigit(str[i])) {
            return false;
        }
    }
    return true;
}

void begin_region() {
    while(mutex);
    mutex = true;
}

void end_region() {
    mutex = false;
}

void process_regular(char *file) {
    char buffer[MAX_BYTE_READ + 1] = {'\0'};
    bool text_flag = true;
    int file_descriptor = open(file, O_RDONLY);
    int total_bytes = 0;
    int bytes_read;

    while((bytes_read = read(file_descriptor, buffer, MAX_BYTE_READ)) > 0) {
        
        // Check if byte is printable
        for(int i = 0; i < bytes_read; i++) {
            if (!(isprint(buffer[i]) || isspace(buffer[i]))) {
                text_flag = false;
            }
        }

        total_bytes += bytes_read;
    }

    // Increment Regular File Size
    begin_region();
    regular_files_size += total_bytes;
    end_region();

    // Increment Text Files and File size
    if(text_flag) {
        begin_region();
        text_files_size += total_bytes;
        text_files++;
        end_region();
    }

    // Close the file
    close(file_descriptor);
}

void serial_func(char *file) {
    struct stat statistic;

    if((stat(file, &statistic)) == 0) {
        // File is good

        if(S_ISDIR(statistic.st_mode)) { 
            // Directory File
            begin_region();
            directories++;
            end_region();
        }
        else if(S_ISREG(statistic.st_mode)) { 
            // Regular File
            begin_region();
            regular_files++;
            end_region();

            process_regular(file);

        }
        else {
            // File is special
            begin_region();
            special_files++;
            end_region();
        }
    }
    else {
        // File is bad
        begin_region();
        bad_files++;
        end_region();
    }
}

void *threadFunction(void *arg) {
    struct stat statistic;
    long idx = (long) arg;

    serial_func(file_arr[idx]);
}

int main(int argc, char *argv[]) {
    // General Variables
    FILE* current_file;
    char *file_list = argv[1];
    char *current_line = new char[MAX_BYTE_READ];
    bool serial_flag = false;
    int working_threads = 0;
    int num_threads;

    // Argument handling
    if(argc == 1) {
        serial_flag = true;
    }
    else if(argc == 3) {
        if (strcmp(argv[1], "thread") == 0) {
            if(!is_integer(argv[2])) {
                cerr << "Error: Invalid first argument" << endl;
                print_help();
            }
            else {
                num_threads = atoi(argv[2]);
                if((num_threads < 1) || (num_threads > MAX_THREADS)){
                    cerr << "Error: Invalid number of threads" << endl;
                    print_help();
                }
            }
        }
    }
    else {
        print_help();
    }

    // Get each file from file list
    while(cin.getline(current_line, MAX_BYTE_READ)) {
        if (!serial_flag) {
            // Wait for available thread
            while (working_threads >= num_threads) {
                begin_region();
                pthread_t current_thread;
                dequeue(&current_thread);
                end_region();

                // Wait for the thread to finish
                (void) pthread_join(current_thread, NULL);
                begin_region();
                working_threads--;
                end_region();
            }
        }

        char* current_file = strtok(current_line, " \n");
        if(current_file == NULL) {
            break;
        }
        
        // Put current file into file array
        begin_region();
        strcpy(file_arr[file_idx], current_file);
        end_region();
        
        if(serial_flag) {
            serial_func(file_arr[file_idx]);
            file_idx = (file_idx + 1) % MAX_THREADS;
        }
        else {
            // Create thread and operate on the files
            if (pthread_create(&thread_arr[file_idx], NULL, threadFunction, (void *) (long) file_idx) != 0) {
                cerr << "Error: pthread creation" << endl;
                exit(1);
            }
 
            // Increment working thread
            begin_region();
            working_threads++;
            enqueue(&thread_arr[file_idx]);
            end_region();

            file_idx = (file_idx + 1) % num_threads;
        }

    }

    // Clean up
    while(!is_empt()) {
        pthread_t current_thread;
        dequeue(&current_thread);
        (void)pthread_join(current_thread, NULL);
    }

    print_results();

    return 0;
}
