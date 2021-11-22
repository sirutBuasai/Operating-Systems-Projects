#include <iostream>
#include <cmath>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/time.h>

using namespace std;

bool is_integer(char *str) {
    for (int i = 0; i < strlen(str); i++) {
        if (!isdigit(str[i])) {
            return false;
        }
    }
    return true;
}

void print_help() {
    cerr << "./proj2 only accepts either 2 or 3 arguments." << endl;
    cerr << "Usage: ./proj2 <source file> <seach character> [optional: <size|mmap>]" << endl;
    cerr << "Example: ./proj2 foo.txt f" << endl;
    cerr << "         ./proj2 foo.txt f 511" << endl;
    cerr << "         ./proj2 foo.txt f mmap" << endl;
    cerr << "         ./proj2 foo.txt f p8" << endl;
    cerr.flush();
}

void print_output(int size, char c, int count){
    cout << "File size: " << size << endl;
    cout << "Count of " << c << " : " << count << endl;
    cout.flush();
}

void print_stats(struct rusage data, struct timeval start_time, struct timeval end_time) {
    double cputime_sec = (data.ru_utime.tv_sec + data.ru_stime.tv_sec) * 1000;
    double cputime_usec = (data.ru_utime.tv_usec + data.ru_stime.tv_usec)*0.001;
    double total_cputime = cputime_sec + cputime_usec;
    
    double time_sec = (end_time.tv_sec - start_time.tv_sec)*1000;
    double time_usec =  (end_time.tv_usec - start_time.tv_usec)*0.001;
    double total_time = time_sec + time_usec;

    cout << "Total CPU time: " << total_cputime << "ms\n";
    cout << "Total Elapsed time: " << total_time << "ms\n";
    cout << "Preempted involuntarily: " << data.ru_nivcsw << endl;
    cout << "Gave up CPU volutarily: " << data.ru_nvcsw << endl;
    cout << "Major page faults: " << data.ru_majflt << endl;
    cout << "Minor page faults: " << data.ru_minflt << endl;
    cout << "Max resident set size: " << data.ru_maxrss << "kB\n";
}

void read_file(char *file_name, char search_char, int max_read) {
    struct rusage usage;
    struct timeval start, end;

    char buffer[max_read + 1] = {'\0'};
    int file_descriptor;
    int file_bytes = 0;
    int file_size = 0;
    int char_count = 0;

    if ((file_descriptor = open(file_name, O_RDONLY)) > 0) {

        // Keep reading the file until end-of-file
        gettimeofday(&start, NULL);
        while ((file_bytes = read(file_descriptor, buffer, max_read)) > 0) {
            file_size += file_bytes;

            // Compare each char to teh search char
            for (int i = 0; i < file_bytes; i++) {
                if (buffer[i] == search_char) {
                    char_count ++;

                }
            }
        }
        

        // Output results
        print_output(file_size, search_char, char_count);

        // Collect statistics
        gettimeofday(&end, NULL);
        getrusage(RUSAGE_SELF, &usage);

        // Print out stats
        print_stats(usage, start, end);

        // Cleanup
        close(file_descriptor);
    }
    else {
        cerr << "Error: Cannot open file" << endl;
        print_help();
    }
}

void mmap_file(char *file_name, char search_char, int num_processes) {
    if (num_processes > 0) {
        struct rusage usage;
        struct timeval start, end;

        int pids[num_processes];
        int pid;
        int status;

        struct stat file_stat;
        int file_descriptor;
        int char_count = 0;
        int bytes_read;
        int file_size;
        int chunk_size;
        char *file_in_memory;

        // Open files and map them to memory
        if ((file_descriptor = open(file_name, O_RDONLY)) > 0) {

            // Get the file statistics from the file
            if (fstat(file_descriptor, &file_stat) == -1) {
                cerr << "Error: Cannot determine file size" << endl;
                print_help();
            }
            else {
                file_size = file_stat.st_size;
                chunk_size = round((double) file_stat.st_size/num_processes);

                // Read the file using mmap in one go with the file size
                file_in_memory = (char*) mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, file_descriptor, 0);
            }
        }
        else {
            cerr << "Error: Cannot open file" << endl;
            print_help();
        }

        // Read through the memory for each child process
        for (int i = 0; i < num_processes; i++) {
            if ((pids[i] = fork()) < 0) {
                cerr << "Error: Cannot fork child process" << endl;
                print_help();
            }
            else if (pids[i] == 0) {
                int curr_start_byte = i*chunk_size;
                int next_start_byte;

                // Child: read through the memory section with all the texts
                // If the child process is the last one, read till the end
                if (i == num_processes-1) {
                    next_start_byte = file_size;
                }
                else {
                    next_start_byte = (i+1)*chunk_size;
                }

                bytes_read = next_start_byte-curr_start_byte;
                for (int j = curr_start_byte; j < next_start_byte; j++) {
                    if (file_in_memory[j] == search_char) {
                        char_count ++;
                    }
                }

                // Output results
                print_output(bytes_read, search_char, char_count);
                close(file_descriptor);
                exit(0);
            }
        }

        // Parent: wait for each child to finish
        gettimeofday(&start, NULL);
        while (num_processes > 0) {
            pid = waitpid(-1, &status, 0);
            num_processes--;
        }
        gettimeofday(&end, NULL);
        getrusage(RUSAGE_CHILDREN, &usage);

        // Print out stats
        print_stats(usage, start, end);

        // Cleanup
        if (munmap(file_in_memory, file_size) < 0) {
            cerr << "Error: Cannot unmap memory" << endl;
            print_help();
        }

    }
    else {
        cout << "Error: invalid number of processes" << endl;
        print_help();
    }
}

int main(int argc, char *argv[]) {
    char *f_name = argv[1];
    char search_c = argv[2][0];
    char *option = argv[3];
    int max_byte = 1024;

    switch (argc)
    {
    case 1:
    {
        print_help();
        break;
    }
    case 2:
    {
        print_help();
        break;
    }
    case 3:
    {
        // Perform operation using read()
        read_file(f_name, search_c, max_byte);
        break;
    }
    case 4:
    {
        // Size option
        if (is_integer(option)) {
            int buffer_int;
            buffer_int = atoi(option);

            // Determine if the input size is out of constraints
            if (0 < buffer_int <= 8192) {
                max_byte = buffer_int;
            }

            // Perform operation using read()
            read_file(f_name, search_c, max_byte);

        }


        // Mmap option
        else if (strcmp(option, "mmap") == 0) {
            // Perform operation using mmap()
            mmap_file(f_name, search_c, 1);

        }


        // Child parallelization option
        else if (option[0] == 'p') {
            option++;
            int no_child = atoi(option);

            // If check if no_child is within constraints
            if (no_child <= 16) {
                mmap_file(f_name, search_c, no_child);

            }
            else {
                cerr << "Error: Invalid number of child" << endl;
                print_help();
            }
        }

        // Invalid third option
        else {
            print_help();
        }
        break;
    }
    default:
    {
        print_help();
        break;
    }
    }
}
