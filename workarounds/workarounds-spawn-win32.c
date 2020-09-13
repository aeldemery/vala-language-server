#include "workarounds.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>
#include <glib.h>

/*
 * see https://docs.microsoft.com/en-us/windows/win32/ProcThread/creating-a-child-process-with-redirected-input-and-output
 * @param error comes from calling GetLastError()
 * @returns a statically-allocated message. don't free it
 */
static const char *
win32_strerror(DWORD error)
{
    static _Thread_local char msgbuf[1024];

    FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&msgbuf[0],
            (DWORD)sizeof(msgbuf),
            NULL);

    return msgbuf;
}

static char * 
read_file_into_string(HANDLE hFile)
{
    size_t alloc_size = 1024;		// total amount allocated
    char *buffer = malloc(alloc_size);
    size_t buf_len = 0;		// total amount read
    DWORD amt_read_once = 0;
    char *buffer_at_offset = buffer;

    while (ReadFile(hFile, buffer_at_offset, alloc_size - buf_len, &amt_read_once, NULL)) {
        buf_len += amt_read_once;
        if (buf_len >= 3*alloc_size/4) {
            // resize
            alloc_size *= 2;
            char *rbuffer = realloc(buffer, alloc_size);
            if (!rbuffer)
                goto realloc_failure;
            buffer = rbuffer;
        }
        buffer_at_offset = buffer + buf_len;
    }

    if (amt_read_once != 0 || buf_len == 0)
        fprintf(stderr, "%s: ReadFile() (possibly) failed - %s", __func__, win32_strerror(GetLastError()));

    char *rbuffer = realloc(buffer, buf_len + 1);
    if (!rbuffer)
        goto realloc_failure;
    buffer = rbuffer;
    buffer[buf_len++] = '\0';

    return buffer;

realloc_failure:
    fprintf(stderr, "%s: realloc() failed\n", __func__);
    free(buffer);
    return NULL;
}

static char *
escape_char(const char *unescaped_str, char to_escape)
{
    g_return_val_if_fail(unescaped_str, NULL);
    int length = 0;
    int to_escape_count = 0;
    int new_length = 0;
    char *new_escaped_str = NULL;

    while (unescaped_str[length]) {
        if (unescaped_str[length] == to_escape)
            to_escape_count++;
        length++;
    }

    new_escaped_str = malloc(length + to_escape_count + 1 /* trailing NUL char */);
    for (int i = 0; i < length; i++)
    {
        if (unescaped_str[i] == to_escape)
            new_escaped_str[new_length++] = '\\';
        new_escaped_str[new_length++] = unescaped_str[i];
    }
    new_escaped_str[new_length] = '\0';

    return new_escaped_str;
}

static void
close_and_invalidate_handle(HANDLE *handle_ptr)
{
    g_return_if_fail(handle_ptr);
    if (*handle_ptr) {
        CloseHandle(*handle_ptr);
        *handle_ptr = NULL;
    }
}

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
    g_return_val_if_fail(argv != NULL, false);
    g_return_val_if_fail(argv[0] != NULL, false);
    // MSVC-specific thread-local keyword: https://docs.microsoft.com/en-us/cpp/parallel/thread-local-storage-tls?view=vs-2019
    // maximum length of command string in CreateProcess is INT16_MAX: https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw
    static _Thread_local char command_string[INT16_MAX];
    int cmdstr_len = 0;

    // the first element is the read end, and the second is the write end of the pipe
    HANDLE pipe_proc_stdin[2] = { 0, 0 };
    HANDLE pipe_proc_stdout[2] = { 0, 0 };
    HANDLE pipe_proc_stderr[2] = { 0, 0 };
    bool createproc_result = false;
    STARTUPINFOA si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    SECURITY_ATTRIBUTES sa = { 0 };	// security attributes for pipes

    sa.nLength = sizeof(sa);
    sa.bInheritHandle = true;

    // create pipes (see https://docs.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createpipe)
    if (!CreatePipe(&pipe_proc_stdin[0], &pipe_proc_stdin[1], &sa, 0))
        return false;
    if (!CreatePipe(&pipe_proc_stdout[0], &pipe_proc_stdout[1], &sa, 0))
        goto cleanup;
    if (!CreatePipe(&pipe_proc_stderr[0], &pipe_proc_stderr[1], &sa, 0))
        goto cleanup;

    // ensure the read handles for the stdout/stderr pipes aren't inherited, by removing the INHERIT flag
    // ensure write handle for stdin pipe isn't inherited
    if (!SetHandleInformation(pipe_proc_stdin[1], HANDLE_FLAG_INHERIT, 0))
        goto cleanup;
    if (!SetHandleInformation(pipe_proc_stdout[0], HANDLE_FLAG_INHERIT, 0))
        goto cleanup;
    if (!SetHandleInformation(pipe_proc_stderr[0], HANDLE_FLAG_INHERIT, 0))
        goto cleanup;

    // see https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/ns-processthreadsapi-startupinfoa
    si.cb = sizeof(si);		// the size of this structure
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = pipe_proc_stdin[0];
    si.hStdOutput = pipe_proc_stdout[1];
    si.hStdError = pipe_proc_stderr[1];

    char *cmdstr_at_offset = command_string;
    cmdstr_len += snprintf(cmdstr_at_offset, sizeof(command_string) - cmdstr_len, "%s", argv[0]);
    cmdstr_at_offset = command_string + cmdstr_len;
    // now create the command string after argv[0]
    for (int arg_i = 1; argv[arg_i] && cmdstr_len + strlen(argv[arg_i]) < sizeof(command_string); arg_i++) {
        char *escaped = escape_char(argv[arg_i], '"');
        cmdstr_len += snprintf(cmdstr_at_offset, sizeof(command_string) - cmdstr_len, " \"%s\"", escaped);
        cmdstr_at_offset = command_string + cmdstr_len;
        free(escaped);
    }
    command_string[cmdstr_len] = '\0';

    // printf("launching this command:\n%s\n...\n", command_string);

    createproc_result = CreateProcessA(NULL, command_string, NULL, NULL, true, CREATE_NO_WINDOW, NULL /* TODO: parse envp */, working_directory, &si, &pi);
    if (!createproc_result) {
        fprintf(stderr, "%s: CreateProcessW() failed - %s\n", __func__, win32_strerror(GetLastError()));
        goto cleanup;
    }

    // close handles that the parent process doesn't need
    close_and_invalidate_handle(&pipe_proc_stdin[0]);
    close_and_invalidate_handle(&pipe_proc_stdout[1]);
    close_and_invalidate_handle(&pipe_proc_stderr[1]);

    // now, read stdout and stderr into string
    if (standard_output)
        *standard_output = read_file_into_string(pipe_proc_stdout[0]);
    if (standard_error)
        *standard_error = read_file_into_string(pipe_proc_stderr[0]);

    // sleep until child process completes
    while (WaitForSingleObject(pi.hProcess, INFINITE) == STILL_ACTIVE)
        ;

    // get return code
    if (exit_status) {
        DWORD exit_status_dword;
        if (!GetExitCodeProcess(pi.hProcess, &exit_status_dword)) {
            fprintf(stderr, "%s: GetExitCodeProcess() failed - %s\n", __func__, win32_strerror(GetLastError()));
            goto cleanup;
        }
        *exit_status = exit_status_dword;
    }

cleanup:
    close_and_invalidate_handle(&pipe_proc_stdin[0]);
    close_and_invalidate_handle(&pipe_proc_stdin[1]);
    close_and_invalidate_handle(&pipe_proc_stdout[0]);
    close_and_invalidate_handle(&pipe_proc_stdout[1]);
    close_and_invalidate_handle(&pipe_proc_stderr[0]);
    close_and_invalidate_handle(&pipe_proc_stderr[1]);
    close_and_invalidate_handle(&pi.hProcess);
    close_and_invalidate_handle(&pi.hThread);

    return createproc_result;
}
