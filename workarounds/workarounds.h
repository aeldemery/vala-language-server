#ifndef WORKAROUNDS_H 
#define WORKAROUNDS_H

#include <stdbool.h>

bool
workarounds_spawn_sync(const char *working_directory,
                       char **argv,
                       /* const char **envp, */
                       char **standard_output,
                       char **standard_error,
                       int *exit_status);

#endif
