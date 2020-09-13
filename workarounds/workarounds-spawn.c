#include "workarounds.h"
#include <glib.h>

/**
 * workarounds_spawn_sync:
 * @working_directory: (type filename) (nullable): the working directory
 * @argv: (array zero-terminated=1) (element-type filename): the argument vector
 * @standard_output: (out) (optional): return location for child output, or %NULL
 * @standard_error: (out) (optional): return location for child error, or %NULL
 * @exit_status: (out) (optional): return location for child exit status, or %NULL
 *
 * Spawns a new process and reads in standard input and output.
 */
bool
workarounds_spawn_sync(const char *working_directory,
                       char **argv,
                       /* const char **envp, */
                       char **standard_output,
                       char **standard_error,
                       int *exit_status)
{
    gboolean retval = FALSE;

    retval = g_spawn_sync(working_directory,
                          argv,
                          NULL,
                          G_SPAWN_SEARCH_PATH,
                          NULL,
                          NULL,
                          standard_output,
                          standard_error,
                          exit_status,
                          NULL /* TODO: implement GError for Win32 version */);

    return retval;
}

