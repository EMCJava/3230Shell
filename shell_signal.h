//
// Created by lys on 10/8/22.
//

#ifndef SHELL_SIGNAL_3230
#define SHELL_SIGNAL_3230

#include <stdio.h>
#include <signal.h>
#include <setjmp.h>

#include "shell.h"

extern struct Shell g_shell;
extern sigjmp_buf g_readline_ctrlc_buf;

void shell_input_signal_handler(int signum) {
    if (signum == SIGINT) {

        // reading input
        printf("\n");
        siglongjmp(g_readline_ctrlc_buf, 1);
    }
}

void shell_child_signal_handler(int signum) {
    if (signum == SIGCLD) {

        struct ProcessStats finished_process;
        while ((finished_process.pid = wait3(&finished_process.status, WNOHANG, &finished_process.rusageStats)) > 0) {
            struct ProcessStats **child_at_array = Find(g_shell.current_child_process_stats, IsPidEqual,
                                                        finished_process.pid);

            // frontend job
            if (child_at_array != NULL) {
                (**child_at_array).status = finished_process.status;
                (**child_at_array).rusageStats = finished_process.rusageStats;

                if (All(g_shell.current_child_process_stats, IsStatusNotEmpty, NULL))
                    // if all frontend job finished
                    FrontGroundJobTerminate(&g_shell);
            } else {
                // should be background job

                struct Array **finished_child_chain = Find(
                        g_shell.background_last_pipe_process_stats, IsArrayLastPidEqual, finished_process.pid);

                // found it!
                if (finished_child_chain != NULL) {

                    struct ProcessStats **background_last_pipe_process = AtLenRel(**finished_child_chain, -1);
                    (**background_last_pipe_process).status = finished_process.status;
                    (**background_last_pipe_process).rusageStats = finished_process.rusageStats;

                    LogExitCode(WEXITSTATUS(finished_process.status), (**background_last_pipe_process).name);

                    int pds[2];
                    pipe(pds);
                    int sob = dup(1);
                    dup2(pds[1], 1);

                    char *string = malloc(sizeof(char) * 256);
                    memset(string, 0, sizeof(char) * 256);

                    printf("[%d] %s ", (**background_last_pipe_process).pid,
                           (**background_last_pipe_process).name);

                    if (!WIFEXITED(finished_process.status)) {
                        LogSignalled(finished_process.status);

                    } else {
                        printf("Done\n");
                    }

                    read(pds[0], string, 256);
                    string[255] = 0;

                    dup2(sob, 1);
                    PushBack(&g_shell.logger, string);

                    // }

                    // clear bg record

                    ApplyToArrayElements(*finished_child_chain, TermProcess);
                    FreeCustomArrayElements(*finished_child_chain, FreeProcessStats);
                    Remove(&g_shell.background_last_pipe_process_stats, *finished_child_chain);
                    FreeArray(*finished_child_chain);
                }
            }
        }
    }
}

#endif //SHELL_SIGNAL_3230
