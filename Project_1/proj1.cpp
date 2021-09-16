#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

#define MAX_INPUT 127
#define MAX_ARGS 31

using namespace std;

void print_output(struct rusage data, struct timeval start, struct timeval end) {
    double cputime_sec = (data.ru_utime.tv_sec + data.ru_stime.tv_sec) * 1000;
    double cputime_usec = (data.ru_utime.tv_usec + data.ru_stime.tv_usec)*0.001;
    double total_cputime = cputime_sec + cputime_usec;
  
    double time_sec = (end.tv_sec - start.tv_sec)*1000;
    double time_usec = (end.tv_usec - start.tv_usec)*0.001;
    double total_time = time_sec + time_usec;

    cout << "Total CPU time: " << total_cputime << "ms\n";
    cout << "Total Elapsed time: " << total_time << "ms\n";
    cout << "Preempted involuntarily: " << data.ru_nivcsw << endl;
    cout << "Gave up CPU volutarily: " << data.ru_nvcsw << endl;
    cout << "Major page faults: " << data.ru_majflt << endl;
    cout << "Minor page faults: " << data.ru_minflt << endl;
    cout << "Max resident set size: " << data.ru_maxrss << "kB\n";
}

int check_background(char *str) {
    if (strcmp(str, "&") == 0) {
        return 1;
    }
    else {
        return 0;
    }
}

int main(int argc, char *argv[])
     /* argc -- number of arguments */
     /* argv -- an array of strings */
{   
    // Common variables
    int pid;
    struct rusage usage;
    struct timeval start, end;
    int child;

    // Background tasks variables
    int bg_num = 0;
    int bg_pid_arr[MAX_ARGS + 1];
    char *bg_cmd_arr[MAX_ARGS + 1];
    int bg_rusage_arr[MAX_ARGS + 1];

    // Shell Mode   
    if (argc == 1) {
        char prompt[MAX_INPUT] = "==>";
        
        while (1) {
            // Input variables
            char input[MAX_INPUT + 1] = {'\0'};
            char *input_ptr = input;
            char *args[MAX_ARGS + 1] = {NULL};
            int arg_len = 0;

            // Switch Case statement variables
            int switch_cmd = 0;
            int no_custom_cmd = 3;
            char *custom_cmd[no_custom_cmd];
            char cmd_0[5] = "exit";
            char cmd_1[3] = "cd";
            char cmd_2[5] = "jobs";
            custom_cmd[0] = cmd_0;
            custom_cmd[1] = cmd_1;
            custom_cmd[2] = cmd_2;

            // Background tasks variables
            int is_background = 0;
            int status;

            // Prompt
            cout << prompt;
            fgets(input, MAX_INPUT, stdin);

            // Ignore empty input
            if (*input_ptr == '\n') {
                continue;
            }

            // Convert input line to list of arguments
            for (int i = 0; i < sizeof(args) && *input_ptr != '\0'; input_ptr++) {

                // Ignore white space input
                if (*input_ptr == ' ') {
                    continue;
                }

                // Break at line break
                if (*input_ptr == '\n') {
                    break;
                }

                // Args is a list of string, each index points to the string
                args[i] = input_ptr;
                while (*input_ptr != '\0' && *input_ptr != ' ' && *input_ptr != '\n') {
                    input_ptr ++;
                }
                *input_ptr = '\0';
                i++;
                arg_len = i;
            }

            // Check and handle background arguments
            is_background = check_background(args[arg_len-1]);
            if (is_background) {
                args[arg_len-1] = NULL;
            }

            // Handle both "built-in" and shell arguments
            for (int i = 0; i < no_custom_cmd; i++) {
                if ((strcmp(args[0], "set") == 0) && (strcmp(args[1], "prompt") == 0) && (strcmp(args[2], "=")== 0)){
                    switch_cmd = 4;
                    break;
                }
                else if (strcmp(args[0], custom_cmd[i]) == 0) {
                    switch_cmd = i+1;
                    break;
                }
                else {
                    switch_cmd = 0;
                }
            }

            if (bg_num > 0) {
                for (int i = 0; i < bg_num; i++) {
                    gettimeofday(&start, NULL);
                    int result = waitpid(bg_pid_arr[i], &status, WNOHANG);
                    gettimeofday(&end, NULL);
                    getrusage(bg_rusage_arr[i], &usage);

                    if ((bg_pid_arr[i] != 0) && (result != 0 )) {
                        cout << bg_pid_arr[i] << " completed\n";
                        print_output(usage, start, end);
                        bg_pid_arr[i] = 0;
                    }
                }
            }            

            switch (switch_cmd)
            {
            case 0:
                // Fork child process and execute program normally
                if ((pid = fork()) < 0) {
                    cerr << "Fork error\n";
                    ::exit(1);
                }
                else if (pid == 0) {
                    /* child process */
                    child = RUSAGE_SELF;
                    ::exit(execvp(args[0], args));
                }

                /* parent */
                // Get data from child process
                if (!is_background) {
                    // Forground process
                    gettimeofday(&start, NULL);
                    waitpid(pid, NULL, 0);
                    gettimeofday(&end, NULL);
                    getrusage(child, &usage);

                    // Print resources output
                    print_output(usage, start, end);
                    
                }
                else {
                    // Background process
                    cout << pid << " " << endl;
                    char buffer[MAX_INPUT] = "";

                    bg_rusage_arr[bg_num] = RUSAGE_SELF;
                    bg_pid_arr[bg_num] = pid;
                    bg_cmd_arr[bg_num] = buffer;
                    strcpy(bg_cmd_arr[bg_num], args[0]);
                    bg_num ++;
                }
                break;

            case 1:
                // Exit shell
                if (bg_num > 0) {
                    int exit_flag = 0;
                    for (int i = 0; i < bg_num; i++) {
                        int result = waitpid(bg_pid_arr[i], &status, WNOHANG);

                        if ((bg_pid_arr[i] != 0) && (result == 0 )) {
                            cout << "Background tasks currently running" << endl;
                            exit_flag = 1;
                            break;
                        }
                    }  
                    if (exit_flag) {
                        break;
                    }
                    else {
                        return 0;
                    }
                }
                else {
                    return 0;
                }

            case 2:
                // Change dir and get data
                gettimeofday(&start, NULL);
                chdir(args[arg_len-1]);
                gettimeofday(&end, NULL);
                getrusage(RUSAGE_SELF, &usage);

                // Print resources output
                print_output(usage, start, end);
                break;

            case 3:
                // Check background processes
                if (bg_num > 0) {
                    for (int i = 0; i < bg_num; i++) {
                        if (bg_pid_arr[i] > 0) {
                            cout << bg_pid_arr[i] << " " << bg_cmd_arr[i] << endl;
                        }
                    }   
                }
                break;

            case 4:
                // Change prompt and get data
                gettimeofday(&start, NULL);
                strcpy(prompt, args[arg_len-1]);
                gettimeofday(&end, NULL);
                getrusage(RUSAGE_SELF, &usage);

                // Print resources output
                print_output(usage, start, end);
                break;

            default:
                break;
            }
        }
    }

    // Argument Mode
    else {
        if ((pid = fork()) < 0) {
            cerr << "Fork error\n";
            ::exit(1);
        }
        else if (pid == 0) {
            /* child process */
            // Turn input arguments into shell arguments
            char *cmd[argc];
            for (int i = 0; i < argc-1; i++) {
                cmd[i] = argv[i+1];
            }
            cmd[argc-1] = NULL;
            
            // Execute shell arguments
            ::exit(execvp(argv[1], cmd));
        }

        /* parent */
        // Get data from child process
        gettimeofday(&start, NULL);
        wait(NULL);
        gettimeofday(&end, NULL);
        getrusage(RUSAGE_CHILDREN, &usage);

        // Print resources output
        print_output(usage, start, end);
        ::exit(0);
    }
}
