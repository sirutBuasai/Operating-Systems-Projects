// Adam Yang CS 3013 Project 4

#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <typeinfo>
#include <fcntl.h>
#include <semaphore.h>
#include <queue>

using namespace std;

#define MAX_BYTE 1024
#define TRUE 1
#define FALSE 0
#define MAX_QUEUE_SIZE 32
#define MAX_NUM_THREADS 15

/* g++ -o proj4 proj4.cpp -lpthread */

/******************************* Global Variables *******************************/
int mutex;
bool serialMode;
int num_allowed_threads;

int num_bad_files = 0;
int num_directories = 0;
int num_regular_files = 0;
int num_special_files = 0;
int total_bytes_reg_files = 0;
int num_reg_files_just_text = 0;
int total_bytes_text_file = 0;

pthread_t thrIDArray[MAX_NUM_THREADS] = {0};
char fileName_Array[MAX_NUM_THREADS][40];
int fileIndex = 0;

pthread_t thrIDQueue[MAX_QUEUE_SIZE];
int queueFront = 0;
int queueBack = MAX_QUEUE_SIZE - 1;
int queueSize = 0;



/**************************** Function Prototypes ****************************/
void print_help(void);
bool isNumber(string s);
void print_results(void);
void BeginRegion(void);
void EndRegion(void);
int openFile(char *fileName);
void defaultFunction(char *fileName);
void *threadFunction(void *arg);
int isFull(void);
int isEmpty(void);
void queueEnqueue(pthread_t *ptr_thrID);
bool queueDequeue(pthread_t *ptr_thrID);




int main(int argc, char *argv[]) {
    int num_active_threads = 0;

    // Checks the validity of the input arguments
    if(argc == 2) {
        serialMode = true;
    }
    else if((argc == 4) && (strcmp(argv[2], "thread") == 0)) {
        serialMode = false;
        if(!isNumber(argv[3])) {
            cout << "Fourth Argument must be valid integer" << endl;
            cout.flush();
            return 0;
        }
        num_allowed_threads = atoi(argv[3]);

        if((num_allowed_threads < 1) || (num_allowed_threads > MAX_NUM_THREADS)) {
            cout << "Number of threads must be between 1 and 15" << endl;
            cout.flush();
            return 0;
        }

    }
    else {
        cout << "Invalid Arguments" << endl;
        cout.flush();
        print_help();
        return 0;
    }

    // Open list of file names
    FILE* file = fopen("fileList.txt", "r");
    if(file == NULL) {
        cout << "File Open Error" << endl;
        cout.flush();
        return 0;
    }

    char *line;
    // For each file name in the file list ...
    while(fgets(line, 50, file)) { // Retrieves a line in file

        /* If number of threads gets to maximum, then wait until 
            at least one cild thread finishes */
        while((num_active_threads >= num_allowed_threads) && (!serialMode)) {
            // Obtain ID of the oldest thread
            BeginRegion();
            pthread_t curr_thrID;
            queueDequeue(&curr_thrID);
            EndRegion();

            // Wait for the thread to finish
            (void)pthread_join(curr_thrID, NULL);

            // Decrement number of active threads
            BeginRegion();
            num_active_threads--;
            EndRegion();
        }

        // Extract out the file name
        const char delimiters[] = " \n";
        char* current_file_name = strtok(line, delimiters);

        // If no file name, then assume end of file and stop reading files
        if(current_file_name == NULL) {
            break;
        }
        
        // Copy file name into the file name array
        BeginRegion();
        strcpy(fileName_Array[fileIndex], current_file_name);
        EndRegion();

        // cout << "Current file is: " << fileName_Array[fileIndex] << endl;

        
        if(serialMode) { // Default mode: NO threads
            defaultFunction(fileName_Array[fileIndex]);
            fileIndex = (fileIndex + 1) % MAX_NUM_THREADS;
        }
        else { // Thread mode
            
            // Create thread and give it appropriate file name
            if (pthread_create(&thrIDArray[fileIndex], NULL, threadFunction, (void *) fileIndex) != 0) {
                perror("pthread_create");
                exit(1);
            }
 
            // Enqueue thread ID and increment number of active threads
            BeginRegion();
            num_active_threads++;
            queueEnqueue(&thrIDArray[fileIndex]);
            EndRegion();

            // Increment the file index and account for array-looping
            fileIndex = (fileIndex + 1) % num_allowed_threads;
        }

    } // End of while loop

    // At this point the program has stopped reading the file
    cout << "File reading is done" << endl;

    // Wait for and join any threads still running
    while(!isEmpty()) {
        pthread_t curr_thrID;
        queueDequeue(&curr_thrID);
        (void)pthread_join(curr_thrID, NULL);
    }

    // Print the values of the counter variables
    print_results();

    return 0;
}

/* Error message for inputs */
void print_help(void) {
    cout << "Without thread EX:  ./proj4 fileList.txt" << endl;
    cout << "With thread EX:     ./proj4 fileList.txt thread 5" << endl;
    cout.flush();
}

/* Checks if input string is an integer */
bool isNumber(string s) {
    for (int i = 0; i < s.length(); i++)
        if (isdigit(s[i]) == false)
            return false;
 
    return true;
}

/* Print the file statistics */
void print_results(void) {
    cout << "Number of bad files: " << num_bad_files << endl;
    cout << "Number of directories: " << num_directories << endl;
    cout << "Number of regular files: " << num_regular_files << endl;
    cout << "Number of special files: " << num_special_files << endl;
    cout << "Total bytes used by regular files: " << total_bytes_reg_files << endl;
    cout << "Number of regular files with only text: " << num_reg_files_just_text << endl;
    cout << "Total bytes used by just text files: " << total_bytes_text_file << endl;
}

/* Function for starting critical region */
void BeginRegion(void) {
    while(mutex);
    mutex = TRUE;
}

/* Function for ending critical region */
void EndRegion(void) {
    mutex = FALSE;
}

/* Open the given file, returns the file descriptor */
int openFile(char *fileName) {
    int file_descriptor = open(fileName, O_RDONLY);
    if(file_descriptor < 0) { 
        cout << "File Open error" << endl;
        cout.flush();
    }
    return file_descriptor;
}

/* The default behavior of the program, no child threads */
void defaultFunction(char *fileName) {

    // File statistics struct
    struct stat statBuffer;

    /* Processes the file for approprite file statistics */
    if((stat(fileName, &statBuffer)) == 0) { // If the file is good ...

        if(S_ISDIR(statBuffer.st_mode)) { // Directory File
            // Increment the directories counter
            BeginRegion();
            num_directories++;
            EndRegion();
        }
        else if(S_ISREG(statBuffer.st_mode)) { // Regular File
            // Increment the regular file counter 
            BeginRegion();
            num_regular_files++;
            EndRegion();

            // Opens the filename
            int file_descriptor = openFile(fileName);

            // initialize the buffer array
            char strBuffer[MAX_BYTE + 1] = {'\0'};

            /* Read the file in chunks and count number of bytes of text */
            bool is_just_text = true; // just-text flag
            int bytes_read = read(file_descriptor, strBuffer, MAX_BYTE);
            while(bytes_read > 0) { // while there are still bytes to read ...
                
                // Temporary byte counter for text
                int textBytes = 0;
                
                /* For every byte read, check if it is printable or a space, if not
                    signal the just-text flag */
                for(int i = 0; i < bytes_read; i++) {
                    if(isprint(strBuffer[i]) || isspace(strBuffer[i])) {
                        textBytes++;
                    }
                    else {
                        is_just_text = false;
                    }
                }

                // Update the necessary global counters
                BeginRegion();
                total_bytes_reg_files += bytes_read;
                total_bytes_text_file += textBytes;
                EndRegion();

                // Read next chunk of bytes
                bytes_read = read(file_descriptor, strBuffer, MAX_BYTE);
            }

            // If file is just text, increment the just-text counter
            if(is_just_text) {
                BeginRegion();
                num_reg_files_just_text++;
                EndRegion();
            }

            // Close the file
            close(file_descriptor);

        }
        else {  // If the file is special ...
            // Increment the special file counter
            BeginRegion();
            num_special_files++;
            EndRegion();
        }
    }
    else {  // If the file is bad
        // Increment the bad file counter
        BeginRegion();
        cout << "Bad File!" << endl;
        num_bad_files++;
        EndRegion();
    }
}


void *threadFunction(void *arg) {
    // File statistics struct
    struct stat statBuffer;
    long providedFileIndex = (long)arg;

    /* Processes the file for approprite file statistics */
    if((stat(fileName_Array[providedFileIndex], &statBuffer)) == 0) { 
        // If the file is good ...

        if(S_ISDIR(statBuffer.st_mode)) { // Directory File
            // Increment the directories counter
            BeginRegion();
            num_directories++;
            EndRegion();
        }
        else if(S_ISREG(statBuffer.st_mode)) { // Regular File
            // Increment the regular file counter 
            BeginRegion();
            num_regular_files++;
            EndRegion();


            // Opens the filename
            int file_descriptor = openFile(fileName_Array[providedFileIndex]);

            // Initialize the buffer array
            char strBuffer[MAX_BYTE + 1] = {'\0'};

            /* Read the file in chunks and count number of bytes of text */
            bool is_just_text = true; // Just-text flag
            int bytes_read = read(file_descriptor, strBuffer, MAX_BYTE);
            while(bytes_read > 0) { // While there are still bytes to read ...
                
                // Temporary byte counter for text
                int textBytes = 0;
                
                /* For every byte read, check if it is printable or a space, if not
                    signal the just-text flag */
                for(int i = 0; i < bytes_read; i++) {
                    if(isprint(strBuffer[i]) || isspace(strBuffer[i])) {
                        textBytes++;
                    }
                    else {
                        is_just_text = false;
                    }
                }

                // Update the necessary global counters
                BeginRegion();
                total_bytes_reg_files += bytes_read;
                total_bytes_text_file += textBytes;
                EndRegion();

                // Read next chunk of bytes
                bytes_read = read(file_descriptor, strBuffer, MAX_BYTE);
            }

            // If file is just text, increment the just-text counter
            if(is_just_text) {
                BeginRegion();
                num_reg_files_just_text++;
                EndRegion();
            }

            // Close the file
            close(file_descriptor);

        }
        else {  // If the file is special ...
            // Increment the special file counter
            BeginRegion();
            num_special_files++;
            EndRegion();
        }
    }
    else {  // If the file is bad
        // Increment the bad file counter
        BeginRegion();
        cout << "Bad File!" << endl;
        num_bad_files++;
        EndRegion();
    }
}


void queueEnqueue(pthread_t *ptr_thrID) {
    if(isFull()) {
        cout << "Queue is full!" << endl;
    }
    else {
        queueBack = (queueBack + 1) % MAX_QUEUE_SIZE;
        thrIDQueue[queueBack] = (*ptr_thrID);
        queueSize++;
    }
}


bool queueDequeue(pthread_t *ptr_thrID) {
    if(queueSize == 0) {
        cout << "Queue is empty!" << endl;
        return 0;
    }
    else {
        (*ptr_thrID) = thrIDQueue[queueFront];

        queueSize--;
        queueFront = (queueFront + 1) % MAX_QUEUE_SIZE;
    }
    return 1;
}

/* Determines if the thread ID queue is full */
int isFull() {
    return (queueSize == MAX_QUEUE_SIZE);
}

/* Determines if the thread ID queue is empty */
int isEmpty() {
    return (queueSize == 0);
}
