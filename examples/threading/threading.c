#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)


void* threadfunc(void* thread_param) {
    struct thread_data* thread_data = (struct thread_data*)thread_param;
    
    // Sleep for 1 millisecond before attempting to obtain the mutex
    usleep(thread_data->wait_to_obtain_ms * 1000);

    // Attempt to obtain the mutex
    if (pthread_mutex_lock(thread_data->mutex) != 0) {
        ERROR_LOG("Failed to obtain the mutex");
        thread_data->thread_complete_success=0;
        return NULL;
    }
    else{
        
        // Wait for a specified amount of time (wait_to_obtain_ms)
        usleep(thread_data->wait_to_release_ms * 1000);

        // Release the mutex
        if (pthread_mutex_unlock(thread_data->mutex) != 0) {
            thread_data->thread_complete_success=0;
            ERROR_LOG("Failed to release the mutex");
        }else{
            thread_data->thread_complete_success=1;
        }

    }
    return thread_param;
}

bool start_thread_obtaining_mutex(pthread_t* thread, pthread_mutex_t* mutex, int wait_to_obtain_ms, int wait_to_release_ms) {

    // Initialize thread data
    struct thread_data *thread_data=malloc(sizeof(struct thread_data));
    thread_data->mutex = mutex;
    thread_data->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_data->wait_to_release_ms = wait_to_release_ms;
    thread_data->thread_complete_success = 0;
    
    // Create a new thread as joinable
    pthread_t t;

    if (pthread_create(&t, NULL, threadfunc, thread_data) != 0) {
        ERROR_LOG("Failed to create the thread");
        return false;
    }
    
    *thread=t;

    return true;
}

