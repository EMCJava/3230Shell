#include <stdio.h>
#include <string.h>
#include "shell.h"
#include "array.h"
#include <setjmp.h>

#define PROCESS_WAIT_INTERVAL 0

int g_long_jumpable = 0;
sigjmp_buf g_ctrlc_buf;

void sigint_handler(int signum) {
    if (signum == SIGINT) {
        if (g_long_jumpable) {
            printf("\n");
            siglongjmp(g_ctrlc_buf, 1);
        }
    }
}

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

    signal(SIGINT, sigint_handler);
    shell.current_command = NULL;
    shell.command_stats_required = 0;
    shell.running = 1;

    return shell;
}

int main(int argc, char *argv[]) {
    struct Shell shell = initShell();
    struct Shell *shell_ptr = &shell;
    while (shell.running) {

        // setting a jump point to exit blocking readline function
        while (sigsetjmp(g_ctrlc_buf, 1) != 0);

        g_long_jumpable = 1;
        printf("$$ 3230shell ##   ");
        acceptCommand(shell_ptr);
        g_long_jumpable = 0;

        interpCommand(shell_ptr);
    }

    freeShell(&shell);
    printf("3230shell: Terminated");
    return 0;
}
