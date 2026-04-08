#include "thread_utils.h"
#include "dslsyntax_log.h"
#include <string.h> /* For memset, though not strictly C90 but widely available */
#include <errno.h>  /* For POSIX error numbers with pthreads */


/* Global mutex and information, static to limit scope to this file */
static MutexType codeblock_mutex;
static MutexType parser_active_mutex;
static int parser_thread_initialized = 0; /* 0 for false, 1 for true */

static ThreadType thread_id;      // Thread ID for the parsing thread
static EventType parse_complete_event;   // This is the event that is signaled when parsing is complete
static int parsing_thread_active; // 0 if no parsing thread is active, 1 if a thread is active

// Structure for thread launching arguments
typedef struct {
    ThreadFunctionType user_routine;
    void *user_arg;
} ThreadWrapperArgs;

/*
 * Initializes the threading utility's global mutex.
 */
int init_parser_thread_utils(void) {
    if (parser_thread_initialized) {
        return 0; /* Already initialized */
    }

#ifdef _WIN32
    codeblock_mutex = CreateMutex(NULL, FALSE, NULL);
    if (codeblock_mutex == NULL) {
        fprintf(stderr, "Failed to create global mutex. Windows Error: %lu\n", GetLastError());
        return -1;
    }
    parser_active_mutex = CreateMutex(NULL, FALSE, NULL);
    if (parser_active_mutex == NULL) {
        fprintf(stderr, "Failed to create parser active mutex. Windows Error: %lu\n", GetLastError());
        CloseHandle(codeblock_mutex); /* Clean up mutex */
        return -1;
    }
#else /* POSIX */
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    if (pthread_mutex_init(&codeblock_mutex, &attr) != 0) {
        perror("Failed to initialize global mutex");
        pthread_mutexattr_destroy(&attr);
        return -1;
    }
    if (pthread_mutex_init(&parser_active_mutex, &attr) != 0) {
        perror("Failed to initialize parser active mutex");
        pthread_mutex_destroy(&codeblock_mutex); /* Clean up mutex */
        pthread_mutexattr_destroy(&attr);
        return -1;
    }
    pthread_mutexattr_destroy(&attr); /* Attributes are no longer needed */
#endif
    parser_thread_initialized = 1;
    parsing_thread_active = 0; // No parsing thread is active initially
    thread_id = 0; // Initialize thread ID to NULL

    // Initialize the parse complete event
    parse_complete_event.initialized = 0;
#ifdef _WIN32
    parse_complete_event.event_handle = NULL;
    /* Create a manual-reset event, initially non-signaled. */
    parse_complete_event.event_handle = CreateEvent(
        NULL,  /* Default security attributes */
        TRUE,  /* Manual-reset event: stays signaled until ResetEvent is called */
        FALSE, /* Initial state is non-signaled */
        NULL   /* Unnamed event object */
    );
    if (parse_complete_event.event_handle == NULL) {
        fprintf(stderr, "Failed to create event object. Windows Error: %lu\n", GetLastError());
        return -1;
    }
#else /* POSIX */
    parse_complete_event.is_set = 0;
    if (pthread_mutex_init(&parse_complete_event.event_mutex, NULL) != 0) {
        perror("Failed to initialize event mutex");
        return -1;
    }
    if (pthread_cond_init(&parse_complete_event.cond_var, NULL) != 0) {
        perror("Failed to initialize event condition variable");
        pthread_mutex_destroy(&parse_complete_event.event_mutex); /* Clean up mutex */
        return -1;
    }
#endif
    parse_complete_event.initialized = 1;
    
    return 0;
}

/*
 * Destroys the threading utility's global mutex.
 */
void destroy_thread_utils(void) {
    if (parser_thread_initialized) {
#ifdef _WIN32
        if (codeblock_mutex != NULL) CloseHandle(codeblock_mutex);
        if (parser_active_mutex != NULL) CloseHandle(parser_active_mutex);
#else /* POSIX */
        pthread_mutex_destroy(&codeblock_mutex);
        pthread_mutex_destroy(&parser_active_mutex);
#endif
        parser_thread_initialized = 0;
    }
    
    // Destroy the parse complete event
    if (parse_complete_event.initialized) {
#ifdef _WIN32
        if (parse_complete_event.event_handle != NULL) {
            CloseHandle(parse_complete_event.event_handle);
            parse_complete_event.event_handle = NULL;
        }
#else /* POSIX */
        pthread_cond_destroy(&parse_complete_event.cond_var);
        pthread_mutex_destroy(&parse_complete_event.event_mutex);
#endif
        parse_complete_event.initialized = 0;
#ifndef _WIN32
        parse_complete_event.is_set = 0;
#endif
    }
}

/*
 * Enter the Critical Section using the global mutex.
*/
int enter_codeblock_critical_section(void) {
#ifdef _WIN32
    DWORD wait_result;
#endif
    if (!parser_thread_initialized) {
        fprintf(stderr, "Error: Global mutex not initialized. Call init_thread_utils first.\n");
        return -1;
    }
#ifdef _WIN32
    wait_result = WaitForSingleObject(codeblock_mutex, INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        fprintf(stderr, "Failed to lock global mutex. Windows Error: %lu\n", GetLastError());
        return -1;
    }
#else /* POSIX */
    if (pthread_mutex_lock(&codeblock_mutex) != 0) {
        perror("Failed to lock global mutex");
        return -1;
    }
#endif
    return 0;
}

/*
 * Exit the Critical Section using the global mutex.
*/
int exit_codeblock_critical_section(void) {
    if (!parser_thread_initialized) {
        /* Or handle error if critical section was entered without init */
        return -1;
    }
#ifdef _WIN32
    if (!ReleaseMutex(codeblock_mutex)) {
        fprintf(stderr, "Failed to release global mutex. Windows Error: %lu\n", GetLastError());
        return -1; /* Indicate failure */
    }
#else /* POSIX */
    if (pthread_mutex_unlock(&codeblock_mutex) != 0) {
        perror("Failed to unlock global mutex");
        return -1; /* Indicate failure */
    }
#endif
    return 0;
}

// Static function to enter the critical section for the parsing thread active flag
static int enter_parse_active_critical_section(void) {
#ifdef _WIN32
    DWORD wait_result;
#endif
    if (!parser_thread_initialized) {
        fprintf(stderr, "Error: Global mutex not initialized. Call init_thread_utils first.\n");
        return -1;
    }
#ifdef _WIN32
    wait_result = WaitForSingleObject(parser_active_mutex, INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        fprintf(stderr, "Failed to lock global mutex. Windows Error: %lu\n", GetLastError());
        return -1;
    }
#else /* POSIX */
    if (pthread_mutex_lock(&parser_active_mutex) != 0) {
        perror("Failed to lock global mutex");
        return -1;
    }
#endif
    return 0;
}

// Static function to exit the critical section for the parsing thread active flag
static int exit_parse_active_critical_section(void) {
    if (!parser_thread_initialized) {
        /* Or handle error if critical section was entered without init */
        return -1;
    }
#ifdef _WIN32
    if (!ReleaseMutex(parser_active_mutex)) {
        fprintf(stderr, "Failed to release global mutex. Windows Error: %lu\n", GetLastError());
        return -1; /* Indicate failure */
    }
#else /* POSIX */
    if (pthread_mutex_unlock(&parser_active_mutex) != 0) {
        perror("Failed to unlock global mutex");
        return -1; /* Indicate failure */
    }
#endif
    return 0;
}

/* Function to check if the parsing thread is active */
int editor_is_parsing_thread_active(void) {

    // Enter Critical Section
    int rc = enter_parse_active_critical_section();
    if (rc != 0) {
        fprintf(stderr, "Failed to enter critical section: %d\n", rc);
        exit(EXIT_FAILURE);
    }
    int active = parsing_thread_active;
    // Exit Critical Section
    rc = exit_parse_active_critical_section();
    if (rc != 0) {
        fprintf(stderr, "Failed to exit critical section: %d\n", rc);
        exit(EXIT_FAILURE);
    }
    return active;
}

#ifdef _WIN32
static DWORD WINAPI parser_thread_wrapper(LPVOID arg_wrapper_pv) {
#else
static void* parser_thread_wrapper(void *arg_wrapper_pv) {
#endif
    ThreadWrapperArgs *wrapper_args = (ThreadWrapperArgs *)arg_wrapper_pv;
    ThreadFunctionType user_routine = wrapper_args->user_routine;
    void *user_arg = wrapper_args->user_arg;
    free(wrapper_args); // Free the dynamically allocated wrapper arguments

#ifdef _WIN32
    DWORD result = user_routine(user_arg); // Call the user's actual thread function
#else
    void* result = user_routine(user_arg); // Call the user's actual thread function
#endif

    // Thread work is complete, now update the active flag
    if (enter_parse_active_critical_section() == 0) {
        parsing_thread_active = 0;
        if (exit_parse_active_critical_section() != 0) {
            fprintf(stderr, "parser_thread_wrapper: Failed to exit critical section after resetting flag.\n");
            // Error already logged by exit_parser_critical_section
        }
    } else {
        fprintf(stderr, "parser_thread_wrapper: Failed to enter critical section to reset parsing_thread_active. Flag may be incorrect.\n");
        // This is a more serious issue, as the flag will remain true.
    }

#ifdef _WIN32
    return result;
#else
    return result; // Or pthread_exit(result);
#endif
}

/*
 * Launches a new thread.
 */
int launch_parser_thread(ThreadFunctionType start_routine, void *arg) {

    if (start_routine == NULL) {
        fprintf(stderr, "Error: start_routine for thread cannot be NULL.\n");
        return -1; /* Invalid argument */
    }

    if (enter_parse_active_critical_section() != 0) {
        // Error already logged by enter_parser_critical_section
        return -1; /* Failed to acquire mutex */
    }

    if (parsing_thread_active) {
        fprintf(stderr, "Error: A parsing thread is already active.\n");
        exit_parse_active_critical_section(); /* Release mutex before returning */
        return -2; /* Indicate thread already running */
    }

    // Reset the parse complete event before starting a new thread
    if (reset_parse_complete_event() != 0) {
        fprintf(stderr, "Failed to reset parse complete event.\n");
        exit_parse_active_critical_section(); /* Release mutex */
        return -1; /* Reset failure */
    }

    // Allocate arguments for the wrapper function
    ThreadWrapperArgs *wrapper_args = (ThreadWrapperArgs *)malloc(sizeof(ThreadWrapperArgs));
    if (!wrapper_args) {
        fprintf(stderr, "Failed to allocate memory for thread wrapper arguments.\n");
        exit_parse_active_critical_section(); /* Release mutex */
        return -1; /* Allocation failure */
    }
    wrapper_args->user_routine = start_routine;
    wrapper_args->user_arg = arg;

#ifdef _WIN32
    thread_id = CreateThread(NULL, 0, parser_thread_wrapper, wrapper_args, 0, NULL);
    if (thread_id == NULL) {
        fprintf(stderr, "Failed to create thread. Windows Error: %lu\n", GetLastError());
        free(wrapper_args); // Clean up allocated memory
        exit_parse_active_critical_section(); /* Release mutex */
        return -1;
    }
#else /* POSIX */
    if (pthread_create(&thread_id, NULL, parser_thread_wrapper, wrapper_args) != 0) {
        perror("Failed to create thread");
        free(wrapper_args); // Clean up allocated memory
        exit_parse_active_critical_section(); /* Release mutex */
        return -1;
    }
#endif

    parsing_thread_active = 1; // Set the flag indicating a thread is now active

    if (exit_parse_active_critical_section() != 0) {
        // Error logged by exit_parser_critical_section.
        // The thread is launched, but exiting the critical section failed.
        // This is a potentially problematic state, but the thread is running.
        return -1;
    }


    LOG("launch_parser_thread: thread launched");
    return 0; // Success
}

/*
 * Joins (waits for) the parser thread to complete its execution. (Implementation as provided by user)
 */
int join_parser_thread() {
#ifdef _WIN32
    DWORD wait_result;
    wait_result = WaitForSingleObject(thread_id, INFINITE);
    if (wait_result == WAIT_FAILED) {
        fprintf(stderr, "Failed to join thread. Windows Error: %lu\n", GetLastError());
        CloseHandle(thread_id);
        return -1;
    }
    CloseHandle(thread_id);
#else /* POSIX */
    if (pthread_join(thread_id, NULL) != 0) {
        perror("Failed to join thread");
        return -1;
    }
#endif
    return 0;
}


/* --- Event Management Implementations --- */

int raise_parse_complete_event(void) {
    // printf("raise_parse_complete_event called\n");
    if (!parse_complete_event.initialized) {
        return -1;
    }
#ifdef _WIN32
    if (!SetEvent(parse_complete_event.event_handle)) {
        fprintf(stderr, "Failed to set event. Windows Error: %lu\n", GetLastError());
        return -1;
    }
#else /* POSIX */
    if (pthread_mutex_lock(&parse_complete_event.event_mutex) != 0) {
        perror("Raise_event: Failed to lock mutex");
        return -1;
    }
    parse_complete_event.is_set = 1;
    /* Wake all threads waiting on this condition.
       pthread_cond_signal() could be used if only one waiter is expected/desired.
       Broadcast is safer for a general-purpose event. */
    if (pthread_cond_broadcast(&parse_complete_event.cond_var) != 0) {
        perror("Raise_event: Failed to broadcast condition");
        pthread_mutex_unlock(&parse_complete_event.event_mutex); /* Still attempt to unlock */
        return -1;
    }
    if (pthread_mutex_unlock(&parse_complete_event.event_mutex) != 0) {
        perror("Raise_event: Failed to unlock mutex");
        return -1; /* Mutex state might be problematic */
    }
#endif
    return 0;
}

int reset_parse_complete_event(void) {
    if (!parse_complete_event.initialized) {
        return -1;
    }
#ifdef _WIN32
    if (!ResetEvent(parse_complete_event.event_handle)) {
        fprintf(stderr, "Failed to reset event. Windows Error: %lu\n", GetLastError());
        return -1;
    }
#else /* POSIX */
    if (pthread_mutex_lock(&parse_complete_event.event_mutex) != 0) {
        perror("Reset_event: Failed to lock mutex");
        return -1;
    }
    parse_complete_event.is_set = 0;
    if (pthread_mutex_unlock(&parse_complete_event.event_mutex) != 0) {
        perror("Reset_event: Failed to unlock mutex");
        return -1;
    }
#endif
    return 0;
}

int check_parse_complete_event(void) {
    if (!parse_complete_event.initialized) {
        return -1; /* Error: not initialized */
    }
#ifdef _WIN32
    DWORD wait_result;
    wait_result = WaitForSingleObject(parse_complete_event.event_handle, 0); /* 0 timeout for non-blocking check */
    if (wait_result == WAIT_OBJECT_0) {
        return 1; /* Event is set */
    } else if (wait_result == WAIT_TIMEOUT) {
        return 0; /* Event is not set */
    } else {
        fprintf(stderr, "Failed to check event status. Windows Error: %lu\n", GetLastError());
        return -1; /* Error */
    }
#else /* POSIX */
    int current_status;
    if (pthread_mutex_lock(&parse_complete_event.event_mutex) != 0) {
        perror("Check_event: Failed to lock mutex");
        return -1; /* Error */
    }
    current_status = parse_complete_event.is_set;
    if (pthread_mutex_unlock(&parse_complete_event.event_mutex) != 0) {
        perror("Check_event: Failed to unlock mutex");
        /* Status was read, but unlock failed. Return status but log error. */
    }
    return current_status; /* 0 or 1 */
#endif
}

int wait_for_parse_complete_event(void) {
    if (!parse_complete_event.initialized) {
        return -1;
    }
#ifdef _WIN32
    DWORD wait_result;
    wait_result = WaitForSingleObject(parse_complete_event.event_handle, INFINITE);
    if (wait_result == WAIT_OBJECT_0) {
        return 0; /* Success, event was signaled */
    } else {
        fprintf(stderr, "Failed to wait for event. Windows Error: %lu\n", GetLastError());
        return -1; /* Error */
    }
#else /* POSIX */
    if (pthread_mutex_lock(&parse_complete_event.event_mutex) != 0) {
        perror("Wait_for_event: Failed to lock mutex");
        return -1;
    }
    while (parse_complete_event.is_set == 0) {
        /* pthread_cond_wait atomically unlocks the mutex and waits.
           It re-locks the mutex before returning. */
        if (pthread_cond_wait(&parse_complete_event.cond_var, &parse_complete_event.event_mutex) != 0) {
            perror("Wait_for_event: Failed to wait on condition");
            pthread_mutex_unlock(&parse_complete_event.event_mutex); /* Attempt to unlock before error exit */
            return -1;
        }
    }
    /* Event is set, mutex is locked */
    if (pthread_mutex_unlock(&parse_complete_event.event_mutex) != 0) {
        perror("Wait_for_event: Failed to unlock mutex");
        return -1; /* Error, though event was processed */
    }
#endif
    return 0; /* Success */
}
