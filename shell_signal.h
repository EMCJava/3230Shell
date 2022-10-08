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

                struct ProcessStats **background_last_pipe_process = Find(
                        g_shell.background_last_pipe_process_stats, IsPidEqual, finished_process.pid);

                // found it!
                if (background_last_pipe_process != NULL) {
                    (**background_last_pipe_process).status = finished_process.status;
                    (**background_last_pipe_process).rusageStats = finished_process.rusageStats;

                    LogExitCode(WEXITSTATUS(finished_process.status), (**background_last_pipe_process).name);

                    printf("[%d] %s ", (**background_last_pipe_process).pid,
                           (**background_last_pipe_process).name);

                    if (!WIFEXITED(finished_process.status)) {
                        LogSignalled(finished_process.status);

                    } else {
                        printf("Done\n");
                    }
                }

                // clear bg record
                if (g_shell.background_last_pipe_process_stats_modifiable) {

                    // check for all, may be didn't clean up precious round
                    for (int i = 0; i < Len(g_shell.background_last_pipe_process_stats); ++i) {
                        void *process_i = *At(g_shell.background_last_pipe_process_stats, i);
                        if (IsStatusNotEmpty(process_i, NULL)) {
                            RemoveAt(&g_shell.background_last_pipe_process_stats, i);
                            FreeProcessStats(process_i);

                            --i;
                        }
                    }
                }
            }
        }
    }
}

#endif //SHELL_SIGNAL_3230
