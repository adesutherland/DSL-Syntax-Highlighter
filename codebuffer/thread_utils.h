//
// thread_utils.h
//

#ifndef THREAD_UTILS_H
#define THREAD_UTILS_H

/* Public header must be platform-neutral. No system headers here. */

/* Opaque/neutral handle types (actual representations are private to .c) */
typedef void *ThreadType;
typedef void *MutexType;

/* Neutral thread function type used by the API */
typedef void *(*ParserThreadFunc)(void *arg);

/*
 * Initializes the threading utility.
 * Must be called before any other thread utility functions.
 * @return 0 on success, -1 on failure.
 */
int init_parser_thread_utils(void);

/*
 * Destroys threading resources allocated by init_parser_thread_utils().
 */
void destroy_thread_utils(void);

/* Returns 1 if a parsing thread is active; 0 otherwise. */
int editor_is_parsing_thread_active(void);

/* Critical section helpers for the codeblock mutex. */
int enter_codeblock_critical_section(void);
int exit_codeblock_critical_section(void);

/*
 * Launch a new parser thread.
 * @param start_routine Function to run in the new thread (neutral signature).
 * @param arg Argument passed to start_routine.
 * @return 0 on success, negative on error.
 */
int launch_parser_thread(ParserThreadFunc start_routine, void *arg);

/* Join (wait for) the parser thread to finish. */
int join_parser_thread(void);

/* --- Event Management (internal single global event) --- */
int raise_parse_complete_event(void);
int reset_parse_complete_event(void);
int check_parse_complete_event(void);
int wait_for_parse_complete_event(void);


#endif /* THREAD_UTILS_H */