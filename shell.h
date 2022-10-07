//
// Created by lys on 10/7/22.
//

#ifndef SHELL_3230
#define SHELL_3230

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "array.h"

struct ProcessStats {
    pid_t pid;
    int status;
    struct rusage rusageStats;
};

int IsPidEqual(void *p_sta, void *pid) {
    return ((struct ProcessStats *) p_sta)->pid == (int) pid;
}

struct Shell {
    char *current_command;
    int command_stats_required;

    // struct ProcessStats
    struct Array current_child_process_stats;

    int running;
};

void freeShell(struct Shell *shell) {
    if (shell->current_command != NULL) {
        free(shell->current_command);
        shell->current_command = NULL;
    }
    shell->running = 0;
}

// pipe split by null
struct Array splitCommand(const char *command) {
    struct Array words = NewArray();
    int is_previous_separator = 1;
    for (const char *ch = command; *ch != 0; ++ch) {
        if (*ch == ' ') is_previous_separator = 1;
        else if (*ch == '|') {
            is_previous_separator = 1;

            //split pipe
            PushBack(&words, NULL);
        } else if (is_previous_separator) {
            is_previous_separator = 0;

            // new parm
            PushBack(&words, 1);
        } else {
            // inside a parm
            ++(*At(words, Len(words) - 1));
        }
    }

    // copy parm
    PushBack(&words, NULL); // mark end
    is_previous_separator = 1;
    int index = 0;
    for (const char *ch = command; *ch != 0; ++ch) {
        if (*ch == ' ') is_previous_separator = 1;
        else if (*ch == '|') {
            if (index == 0) {
                FreeArray(&words);

                printf("3230shell: should not have | before all command\n");
                return NewArray();
            } else if (*At(words, index - 1) == NULL) {
                // double pipe

                for (int i = 0; i < index; ++i)
                    if (*At(words, i))
                        free(*At(words, i));

                FreeArray(&words);

                printf("3230shell: should not have two consecutive | without in-between command\n");
                return NewArray();
            }

            is_previous_separator = 1;
            ++index;
        } else if (is_previous_separator) {
            is_previous_separator = 0;

            int word_length = *At(words, index);

            // allocate str to copy
            *At(words, index) = malloc((word_length + 1) * sizeof(char));

            // copy str
            for (int i = 0; i < word_length; ++i)
                ((char *) *At(words, index))[i] = ch[i];

            // end char
            ((char *) *At(words, index))[word_length] = 0;
            ++index;
        }
    }

    if (Len(words) >= 2 && *At(words, Len(words) - 2) == NULL) {
        FreeArray(&words);

        printf("3230shell: should not have | after all command\n");
        return NewArray();
    }

    return words;
}

// return data read
ssize_t PipeReadWrite(int in, int out) {

    char buffer[512];
    ssize_t date_read = read(in, buffer, sizeof(buffer));
    if (date_read > 0) write(out, buffer, date_read);

    return date_read;
}

void runExec(struct Shell *shell, const struct Array words) {

    int childCount = CountNull(words);

    // new child process storage
    FreeArrayElements(&shell->current_child_process_stats);
    FreeArray(&shell->current_child_process_stats);
    shell->current_child_process_stats = NewArray();

    pid_t last_child;
    int current_pipe_in = -1;
    int commandPipeIndex = 0;

    int stdin_cpy = dup(0);
    int stdout_cpy = dup(1);
    for (int i = 0; i < childCount; ++i) {

        int previous_pipe_in = current_pipe_in;
        int pipefd[2] = {current_pipe_in, -1};

        if (i != childCount - 1) {
            pipe(pipefd);
            current_pipe_in = pipefd[0];
        } else {
            pipefd[0] = pipefd[1] = -1;
        }

        if (!(last_child = fork())) {
            // child process

            // clean parent stats
            FreeArrayElements(&shell->current_child_process_stats);
            FreeArray(&shell->current_child_process_stats);

            // redirect previous stdout to this input
            if (previous_pipe_in != -1) {
                dup2(previous_pipe_in, 0);
                close(previous_pipe_in);
            }

            // redirect stdout to next input
            if (pipefd[1] != -1) {
                dup2(pipefd[1], 1);
                close(pipefd[1]);
            }

            execvp(words.data[commandPipeIndex], (char **) words.data + commandPipeIndex);
            exit(errno);
        }

        if (previous_pipe_in != -1) close(previous_pipe_in);
        if (pipefd[1] != -1) close(pipefd[1]);

        // ready to record the stats
        struct ProcessStats *finished_process = malloc(sizeof(struct ProcessStats));
        finished_process->pid = last_child;
        PushBack(&shell->current_child_process_stats, finished_process);

        // advance to next pipe
        while (words.data[commandPipeIndex++]);

        // parent process keep creating
    }

    struct ProcessStats last_process;
    while ((last_process.pid = wait3(&last_process.status, WNOHANG, &last_process.rusageStats)) >= 0) {
#if PROCESS_WAIT_INTERVAL != 0
        usleep(PROCESS_WAIT_INTERVAL);
#endif

        // save child stats
        if (last_process.pid > 0) {
            struct ProcessStats **child_at_array = Find(shell->current_child_process_stats, IsPidEqual,
                                                        last_process.pid);
            assert(child_at_array != NULL);
            **child_at_array = last_process;
        }
    }

    // restore stdin/out
    dup2(stdin_cpy, 0);
    dup2(stdout_cpy, 1);

    // exit code
    for (int i = 0; i < Len(shell->current_child_process_stats); ++i)
        if (((struct ProcessStats *) *At(shell->current_child_process_stats, i))->pid == last_child)
            last_process = *(struct ProcessStats *) *At(shell->current_child_process_stats, i);
    int exit_code = WEXITSTATUS(last_process.status);
    if (exit_code != 0) {
        switch (exit_code) {
            case 1: // simply program return 1
                break;
            case 2:
                printf("3230shell: '%s': No such file or directory\n", (char *) *At(words, 0));
                break;
            case 13:
                printf("3230shell: '%s': Permission denied\n", (char *) *At(words, 0));
                break;
            default:
                printf("3230shell: Unknown error code (%d)\n", exit_code);
        }
    }

    // signalled
    if (!WIFEXITED(last_process.status))
        printf("terminated by signal: %d\n", WTERMSIG(last_process.status));

    // stats
    if (shell->command_stats_required) {
        commandPipeIndex = 0;
        for (int i = 0; i < childCount; ++i) {

            // int exit_value = execvp(words.data[commandPipeIndex], (char **) words.data + commandPipeIndex);
            const char *exe_name = strrchr(words.data[commandPipeIndex], '/');
            if (exe_name) ++exe_name;
            else exe_name = words.data[commandPipeIndex];

            last_process = *(struct ProcessStats *) *At(shell->current_child_process_stats, i);

            printf("(PID)%d  (CMD)%s    (user)%.3f s  (sys)%.3f s\n", last_process.pid, exe_name,
                   last_process.rusageStats.ru_utime.tv_sec + last_process.rusageStats.ru_utime.tv_usec / 1000000.0,
                   last_process.rusageStats.ru_stime.tv_sec + last_process.rusageStats.ru_stime.tv_usec / 1000000.0);

            // advance to next command
            while (words.data[commandPipeIndex++]);
        }
    }

    // clean child stats
    FreeArrayElements(&shell->current_child_process_stats);
    FreeArray(&shell->current_child_process_stats);
}

void interpCommand(struct Shell *shell) {
    if (shell->current_command != NULL) {

        shell->command_stats_required = 0;
        struct Array words = splitCommand(shell->current_command);
        struct Array decorated_words = words;

        // empty command
        if (Len(words) == 0 || *At(words, 0) == NULL) {
            FreeArrayElements(&words);
            FreeArray(&words);
            return;
        }

        // default command

        // exit command
        if (Len(words) == 2 && strcmp((char *) *At(words, 0), "exit") == 0) {
            shell->running = 0;
            return;
        }

        // timeX command
        if (strcmp((char *) *At(words, 0), "timeX") == 0) {

            if (Len(words) == 2) {
                printf("3230shell: \"timeX\" cannot be a standalone command\n");
                return;
            }

            if (Len(words) > 2 &&
                strcmp((char *) *At(words, Len(words) - 2), "&") == 0) {
                printf("3230shell: \"timeX\" cannot be run in background mode\n");
                return;
            }

            // skip the first timeX
            decorated_words.data++;
            decorated_words.len--;
            shell->command_stats_required = 1;
        }

        // user program
        runExec(shell, decorated_words);

        FreeArrayElements(&words);
        FreeArray(&words);
    }
}

#endif