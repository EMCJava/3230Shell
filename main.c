/*
 *
 * Name:     Lo Yu Sum
 * UID:      3035786603
 * Platform: Linux gcc( 12.2.0 )
 * Progress: All completed
 *
 * */


#include <stdio.h>
#include <string.h>
#include "shell.h"
#include "array.h"

#include "shell_signal.h"

sigjmp_buf g_readline_ctrlc_buf;

struct Shell g_shell;
int g_sig1 = 0;

// handle input
void acceptCommand(struct Shell *shell) {
    if (shell->current_command != NULL)
        free(shell->current_command);

    size_t len = 0;
    ssize_t lineSize = 0;
    lineSize = getline(&shell->current_command, &len, stdin);
    assert(shell->current_command != NULL);

    // remove new line
    shell->current_command[lineSize - 1] = 0;
}


struct Shell initShell() {
    struct Shell shell;

    shell.current_command = NULL;
    shell.command_stats_required = 0;
    shell.running = 1;
    shell.background_last_pipe_process_stats = NewArray();
    shell.current_child_process_stats = NewArray();
    shell.logger = NewArray();

    signal(SIGCLD, shell_child_signal_handler);

    return shell;
}

int main(int argc, char *argv[]) {
    g_shell = initShell();
    struct Shell *shell_ptr = &g_shell;
    while (g_shell.running) {

        // log

        sigset_t child_mask;
        if ((sigemptyset(&child_mask) == -1) || (sigaddset(&child_mask, SIGCLD) == -1)) {
            return -1;
        }
        if (sigprocmask(SIG_BLOCK, &child_mask, NULL) != -1) {

            for (int i = 0; i < Len(g_shell.logger); ++i) {
                printf("%s", (char *) *At(g_shell.logger, i));
            }

            FreeArrayElements(&g_shell.logger);
            FreeArray(&g_shell.logger);

            while (sigprocmask(SIG_UNBLOCK, &child_mask, NULL) == -1);
        }

        // setting a jump point to exit blocking readline function
        signal(SIGINT, shell_input_signal_handler);
        while (sigsetjmp(g_readline_ctrlc_buf, 1) != 0);
        printf("$$ 3230shell ##   ");
        acceptCommand(shell_ptr);
        signal(SIGINT, SIG_IGN);

        interpCommand(shell_ptr);
    }

    freeShell(&g_shell);
    printf("3230shell: Terminated\n");
    return 0;
}
