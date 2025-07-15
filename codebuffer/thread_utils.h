//
// thread_utils.h
//

#ifndef THREAD_UTILS_H
#define THREAD_UTILS_H

/* Standard C90 includes */
#include <stdio.h>
#include <stdlib.h> /* For malloc/free, though not directly used in this header */

/* Platform-specific includes and type definitions */
#ifdef _WIN32
#include <windows.h>
typedef HANDLE ThreadType;
typedef HANDLE MutexType; /* This is for the global critical section mutex */
/* Define ThreadFunctionType to match Windows CreateThread */
typedef DWORD (WINAPI *ThreadFunctionType)(LPVOID lpThreadParameter);
#else /* POSIX (Linux, macOS) */
#include <unistd.h>
#include <pthread.h>
typedef pthread_t ThreadType;
typedef pthread_mutex_t MutexType; /* This is for the global critical section mutex */
/* Define ThreadFunctionType to match pthread_create */
typedef void *(*ThreadFunctionType)(void *arg);
#endif

/*
 * Initializes the threading utility.
 * This function must be called once before any other thread utility functions.
 * It sets up a global mutex for the callback and stores the callback function pointer.
 *
 * @return 0 on success, -1 on failure (e.g., mutex creation failed).
 */
int init_parser_thread_utils(void);

/*
 * Destroys the threading utility resources.
 * This function should be called when the threading utilities are no longer needed,
 * typically at the end of the program, to release the mutex.
 */
void destroy_thread_utils(void);

/* Function to check if the parsing thread is active */
int editor_is_parsing_thread_active();

/*
 * Enter Critical Section
    * This function is used to enter a critical section by locking the mutex.
*/
int enter_codeblock_critical_section(void);

/*
 * Exit Critical Section
 * This function is used to exit a critical section by unlocking the mutex.
*/
int exit_codeblock_critical_section(void);

/*
 * Launches a new thread in a cross-platform manner.
 *
 * @param start_routine The function to be executed by the new thread.
 *                      The function signature must match ThreadFunctionType
 *                      (DWORD (WINAPI *)(LPVOID) for Windows,
 *                       void *(*)(void *) for POSIX).
 * @param arg           The argument to be passed to the start_routine.
 * @return 0 on success, -1 on failure (e.g., thread creation failed).
 */
int launch_parser_thread(ThreadFunctionType start_routine, void *arg);

/*
 * Joins (waits for) a specified thread to complete its execution.
 * This function blocks the calling thread until the specified thread terminates.
 * For Windows, this also closes the thread handle after joining.
 *
 * @return 0 on success, -1 on failure.
 */
int join_parser_thread(void);


/* --- Event Management --- */

typedef struct {
#ifdef _WIN32
    HANDLE event_handle;      /* Windows Event Object handle */
#else
    pthread_mutex_t event_mutex; /* Mutex for protecting condition variable and flag */
    pthread_cond_t cond_var;   /* POSIX condition variable */
    int is_set;                /* Flag to indicate if the event is set (0 or 1) */
#endif
    int initialized;           /* Flag to indicate if this event object is initialized (0 or 1) */
} EventType;

/*
 * Raises (sets/signals) an event.
 * This will change the event's state to 'set' and notify waiting threads.
 *
 * @return 0 on success, -1 if not initialized or on failure.
 */
int raise_parse_complete_event();

/*
 * Resets an event to its non-raised (not set) state.
 * Allows the event to be waited upon again.
 *
 * @return 0 on success, -1 if not initialized or on failure.
 */
int reset_parse_complete_event();

/*
 * Checks if an event has been raised (non-blocking).
 *
 * @return 1 if the event is set, 0 if not set, -1 on error (e.g., not initialized).
 */
int check_parse_complete_event();

/*
 * Waits until an event is raised (blocking).
 * The function will block until another thread calls raise_cross_platform_event() on this event.
 * If the event is already set when this function is called, it may return immediately.
 *
 * @return 0 on success (event was raised), -1 on error (e.g., not initialized, wait failed).
 */
int wait_for_parse_complete_event();


#endif /* THREAD_UTILS_H */