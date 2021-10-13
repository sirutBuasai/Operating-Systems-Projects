// CS-3013 Assignment 3
// By Sirut Buasai
// sbuasai2@wpi.edu

#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>

using namespace std;

#define MAXTHREAD 10
#define MAX_INPUT 64
#define MAX_QUEUE 32
#define REQUEST 1
#define REPLY 2

struct msg {
    int iFrom;
    int value;
    int count;
    int total_time;
};

struct nb_queue {
    msg message;
    int to_thread;
};

/* ------------------------------------------- Global Variables ------------------------------------------- */

int mutex;
int q_front = 0;
int q_rear = MAX_QUEUE-1;
int q_size = 0;
int blocked_size = 0;
msg mailbox_arr[MAXTHREAD + 1];
pthread_t thread_id_arr[MAXTHREAD + 1];
pthread_t blocked_arr[MAXTHREAD + 1] = { 0 };
sem_t sem_arr[MAXTHREAD + 1];
nb_queue msg_queue[MAX_QUEUE];

/* ------------------------------------------- Queue ------------------------------------------- */

bool is_empt() {
    return (q_size == 0);
}

bool is_full() {
    return (q_size == MAX_QUEUE);
}

void enqueue(struct nb_queue *message) {
    if (is_full()) {
        cerr << "Queue is full" << endl;
        cerr.flush();
    }
    else {
        q_rear = (q_rear + 1) % MAX_QUEUE;
        msg_queue[q_rear] = *message;
        q_size++;
    }
}

void dequeue(struct nb_queue *q_message) {
    if(is_empt()) {
        cerr << "Queue is empty" << endl;
        cerr.flush();
    }
    else {
        q_message->message.iFrom = msg_queue[q_front].message.iFrom;
        q_message->message.value = msg_queue[q_front].message.value;
        q_message->message.count = msg_queue[q_front].message.count;
        q_message->message.total_time = msg_queue[q_front].message.total_time;
        q_message->to_thread = msg_queue[q_front].to_thread;
        q_size--;
        q_front = (q_front + 1) % MAX_QUEUE;
    }
}

/* ------------------------------------------- Functions ------------------------------------------- */

void print_help() {
    cerr << "./proj3 only accepts either 2 or 3 arguments." << endl;
    cerr << "Usage: ./proj3 <number> [optional: \"nb\"]" << endl;
    cerr << "Example: ./proj3 3" << endl;
    cerr << "         ./proj3 3 nb" << endl;
    cerr.flush();
    exit(1);
}

void InitMailbox(int num_thread) {
    // Initialize semaphores
    for(int i = 0; i <= num_thread; i++) {
        if(i == 0) {
            sem_init(&sem_arr[i], 0, num_thread);
        }
        else {
            int val = 0;
            sem_init(&sem_arr[i], 0, 0);
        }
    }
    
    // Initializes mutex
    mutex = 0;

    // Initializes the mailbox array
    for(int i = 0; i <= num_thread; i++) {
        mailbox_arr[i].iFrom = -1;
        mailbox_arr[i].value = '\0';
        mailbox_arr[i].count = '\0';
        mailbox_arr[i].total_time = '\0';
    }
}

void begin_region() {
    while(mutex);
    mutex = true;
}

void end_region() {
    mutex = false;
} 

bool is_integer(char *str) {
    for (int i = 0; i < strlen(str); i++) {
        if (!isdigit(str[i])) {
            return false;
        }
    }
    return true;
}

bool check_blocked_arr(int thread_id) {
    for (int i = 0; i < blocked_size + 1; i++) {
        if (blocked_arr[i] == thread_id) {
            return true;
        }
    }
    return false;
}

void remove_blocked_id(int thread_id) {
    for (int i = 0; i < blocked_size + 1; i++) {
        if (blocked_arr[i] == thread_id) {
            blocked_arr[i] = 0;
        }
    }
}

int NBSendMsg(int to_mailbox, struct msg *message) {
    // If mailbox is not empty return -1
    if(mailbox_arr[to_mailbox].iFrom >= 0) {
        return -1;
    }

    // sem_wait until it is the sender's turn to send, decrements sem_count
    sem_wait(&sem_arr[(*message).iFrom]);

    begin_region();
    // Put the message into the to_mailbox
    mailbox_arr[to_mailbox].iFrom = message->iFrom;
    mailbox_arr[to_mailbox].value = message->value;
    mailbox_arr[to_mailbox].count = message->count;
    mailbox_arr[to_mailbox].total_time = message->total_time;
    end_region();

    // Signal the receiver semaphore
    sem_post(&sem_arr[to_mailbox]);
    return 0;
}

void SendMsg(int to_mailbox, struct msg *message) {
    // sem_wait until it is the sender's turn to send, decrements sem_count
    sem_wait(&sem_arr[message->iFrom]);

    // Blocks until mailbox is empty
    while(mailbox_arr[to_mailbox].iFrom >= 0);

    begin_region();
    // Put the message into the to_mailbox
    mailbox_arr[to_mailbox].iFrom = message->iFrom;
    mailbox_arr[to_mailbox].value = message->value;
    mailbox_arr[to_mailbox].count = message->count;
    mailbox_arr[to_mailbox].total_time = message->total_time;
    end_region();

    // Signal the receiver semaphore
    sem_post(&sem_arr[to_mailbox]);
}

void RecvMsg(int own_mailbox, struct msg *message) {
    // sem_wait until it is the receivers's turn to get the message, decrements sem_count
    sem_wait(&sem_arr[own_mailbox]);
    
    // Blocks until the mailbox has a message
    while(mailbox_arr[own_mailbox].iFrom < 0);
    
    begin_region();
    // Put the recevied message into the thread
    message->iFrom = mailbox_arr[own_mailbox].iFrom;
    message->value = mailbox_arr[own_mailbox].value;
    message->count = mailbox_arr[own_mailbox].count;
    message->total_time = mailbox_arr[own_mailbox].total_time;
    
    // Flag mailbox as empty
    mailbox_arr[own_mailbox].iFrom = -1;
    end_region();

    // Signal the sender semaphore
    sem_post(&sem_arr[message->iFrom]);
}

void *adder(void *arg) {
    int thread_id = (long) arg;
    int exit_flag = false;
    int sum_value = 0;
    int total_count = 0;
    int total_exec_time = 0;
    long start_time, end_time;
    msg thread_msg;
    
    start_time = time(NULL);
    while(!exit_flag) {
        // Receive message
        RecvMsg(thread_id, &thread_msg);

        if(thread_msg.value < 0) {
            exit_flag = true;
        }
        else {
            sum_value += thread_msg.value;
            total_count++;
            sleep(1);
        }
    }
    end_time = time(NULL);
    total_exec_time = end_time - start_time;

    // Put thread info in the message
    thread_msg.iFrom = thread_id;
    thread_msg.value = sum_value;
    thread_msg.count = total_count;
    thread_msg.total_time = total_exec_time;

    // Send message to the main thread
    SendMsg(0, &thread_msg);
}

void create_threads(int num_thread) {
    for(int i = 1; i <= num_thread; i++) {
        if (pthread_create(&thread_id_arr[i], NULL, adder, (void *)(long) i) != 0) {
            cerr << "pthread_create error" << endl;
            print_help();
        }
    }
}  

/* ------------------------------------------- Main Function ------------------------------------------- */

int main(int argc, char *argv[]) {
    // General Variables
    FILE* file;
    int num_threads;
    int sem_count;
    int keep_reading = true;
    char file_input[MAX_INPUT];
    bool non_block = false;
    
    // Argument handling
    if (argc >= 2) {
        // Handle first argument
        if(!is_integer(argv[1])) {
            cerr << "Invalid first argument" << endl;
            print_help();
        }
        else {
            num_threads = atoi(argv[1]);
            if (num_threads > MAXTHREAD){
                cerr << "Number of threads exceeds maximum thread" << endl;
                print_help();
            }
        }

        if (argc == 3) {
            // Handle second argument
            if (strcmp(argv[2], "nb") == 0) {
                non_block = true;
            }
            else{
                cerr << "Invalid second argument" << endl;
                print_help();
            }
        }
    }
    else {
        print_help();
    }

    // Initializations
    InitMailbox(num_threads);
    create_threads(num_threads);
    
    // Open test files
    if ((file = fopen("foo.txt", "r")) == NULL) {
        cerr << "File Open Error" << endl;
        print_help();
    }

    // File input handling
    while (keep_reading) { 
        // Get each line of the file
        fgets(file_input, MAX_INPUT, file); 

        // Reading variables
        char *input_ptr;
        int num_count = 0;
        int num_arr[MAX_INPUT] = {'\0'};
        
        input_ptr = strtok(file_input, " .!@#$%^&*()_,?/\n");
        while (input_ptr != NULL)
        {
            if(is_integer(input_ptr)) {
                num_arr[num_count] = atoi(input_ptr);
                num_count++;
            }
            else {
                num_count++;
                keep_reading = false;
                break;

            }
            input_ptr = strtok(NULL, " !@#$%^&*()_,./?\n");
        }

        // If line does not have two numbers
        if ((num_count != 2) || !keep_reading) {
            break;
        }

        cout << "Current Line is: ";
        for(int i = 0; i < num_count; i++) {
            cout << num_arr[i] << " ";
        }
        cout << endl;

        // Check if sending thread exist
        int payload = num_arr[0];
        int recving_thread = num_arr[1];
        if ((0 < recving_thread) && ( recving_thread <= num_threads) && (payload > 0)) {
            // Message construction
            msg curr_msg;
            curr_msg.iFrom = 0;
            curr_msg.value = payload;
            curr_msg.count = '\0';
            curr_msg.total_time = '\0';

            if (non_block) {
                int result;
                if((result = NBSendMsg(recving_thread, &curr_msg)) == -1) {
                    nb_queue q_msg;
                    q_msg.message.iFrom = curr_msg.iFrom;
                    q_msg.message.value = curr_msg.value;
                    q_msg.message.count = curr_msg.count;
                    q_msg.message.total_time = curr_msg.total_time;
                    q_msg.to_thread = recving_thread;
                    begin_region();
                    enqueue(&q_msg);
                    end_region();

                    if (!check_blocked_arr(recving_thread)) {
                        blocked_arr[blocked_size] = recving_thread;
                        blocked_size++;
                    }
                }
            }
            else {
                SendMsg(recving_thread, &curr_msg);
            }
        }
    }

    // Close file
    fclose(file);

    // Terminating message
    msg terminating_msg;
    terminating_msg.iFrom = 0;
    terminating_msg.value = -1;
    terminating_msg.count = '\0';
    terminating_msg.total_time = '\0';

    // Send terminating message to unblocked threads
    for(int i = 1; i <= num_threads; i++) {
        // Send message to all non-main thread
        if (non_block) {
            int output;
            if (!check_blocked_arr(i)){
                while((output = NBSendMsg(i, &terminating_msg)) < 0);
            }
        }
        else {
            SendMsg(i, &terminating_msg);
        }
    }

    if (non_block) {
        nb_queue msg_to_send;
        int result;
        while(!is_empt()) {
            begin_region();
            dequeue(&msg_to_send);
            end_region();

            if((result = NBSendMsg(msg_to_send.to_thread, &(msg_to_send.message))) != 0) {
                begin_region();
                enqueue(&msg_to_send);
                end_region();
            }
        }

        // Send terminating message to blocked threads
        for(int i = 0; i < blocked_size; i++) {
            // Send message to all blocked thread
            int output;
            while((output = NBSendMsg(blocked_arr[i], &terminating_msg)) < 0);
        }
    }

    // Give all non-main threads its semaphores back.
    for(int i = 1; i <= num_threads; i++) {
        sem_wait(&sem_arr[0]);
        sem_post(&sem_arr[i]);
    }
    
    // Receive termination messages from all threads
    for(int i = 1; i <= num_threads; i++) {
        // Receive message from all non-main thread
        msg result_msg;
        RecvMsg(0, &result_msg);
        int thread_id = result_msg.iFrom;
        int final_sum = result_msg.value;
        int final_count = result_msg.count;
        int final_total_time = result_msg.total_time;

        // Print thread statistics
        cout << "The result from thread " 
        << thread_id << " is "
        << final_sum << " from "
        << final_count << " operations during "
        << final_total_time << " secs." << endl;
    }

    // Clean up threads and semaphores
    for(int i = 1; i <= num_threads; i++) {
        pthread_join(thread_id_arr[i], NULL);
        sem_destroy(&(sem_arr[i]));
    }
    
    return 0;
}
