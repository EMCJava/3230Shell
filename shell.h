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
#include <fcntl.h>

#include "array.h"

#define PROCESS_WAIT_INTERVAL 1000

struct ProcessStats {
    pid_t pid;
    char *name;
    int status;
    struct rusage rusageStats;
};

void TermProcess(void *p_sta) {
    kill(((struct ProcessStats *) p_sta)->pid, SIGKILL);
}

void FreeProcessStats(void *p_sta) {
    free(((struct ProcessStats *) p_sta)->name);
    free(p_sta);
}

int IsPidEqual(void *p_sta, void *pid) {
    return ((struct ProcessStats *) p_sta)->pid == (int) pid;
}

int IsArrayLastPidEqual(void *p_sta, void *pid) {
    return IsPidEqual(*AtLenRel(*((struct Array *) p_sta), -1), (int) pid);
}


int IsStatusNotEmpty(void *p_sta, void *pid) {
    return ((struct ProcessStats *) p_sta)->status != -1;
}

extern int g_sig1;

void user_sig1_handler(int signum) {
    if (signum == SIGUSR1) g_sig1 = 1;
}

struct Shell {
    char *current_command;
    struct Array current_command_list;

    int command_stats_required;
    int command_background_required;

    // struct ProcessStats
    struct Array current_child_process_stats;

    // Array of struct ProcessStats
    struct Array background_last_pipe_process_stats;

    // char*
    struct Array logger;

    int running;
};

void freeShell(struct Shell *shell) {
    if (shell->current_command != NULL) {
        free(shell->current_command);
        shell->current_command = NULL;
    }

    // free all unfinished child
    sigset_t child_mask;
    while ((sigemptyset(&child_mask) == -1) || (sigaddset(&child_mask, SIGCLD) == -1));
    if (sigprocmask(SIG_BLOCK, &child_mask, NULL) != -1) {

        for (int i = 0; i < Len(shell->background_last_pipe_process_stats); ++i) {

            struct Array **child_chain = At(shell->background_last_pipe_process_stats, i);

            ApplyToArrayElements(*child_chain, TermProcess);
            FreeCustomArrayElements(*child_chain, FreeProcessStats);
            Remove(&shell->background_last_pipe_process_stats, *child_chain);
            FreeArray(*child_chain);
        }

        FreeArray(&shell->background_last_pipe_process_stats);

        while (sigprocmask(SIG_UNBLOCK, &child_mask, NULL) == -1);
    }


    shell->running = 0;
}

// pipe split by null
void splitCommand(struct Shell *shell) {
    struct Array words = NewArray();
    int is_previous_separator = 1;
    shell->command_background_required = 0;
    for (const char *ch = shell->current_command; *ch; ++ch) {
        if (*ch == ' ') is_previous_separator = 1;
        else if (*ch == '|') {
            is_previous_separator = 1;

            //split pipe
            PushBack(&words, NULL);
        } else if (*ch == '&') {
            shell->command_background_required = 1;
            while (*++ch) {
                if (*ch != ' ') {
                    FreeArray(&words);

                    printf("3230shell: '&' should not appear in the middle of the command line\n");
                    shell->current_command_list = NewArray();
                    return;
                }
            }
        } else if (is_previous_separator) {
            is_previous_separator = 0;

            // new parm
            PushBack(&words, 1);
        } else {
            // inside a parm
            ++(*AtLenRel(words, -1));
        }
    }

    // copy parm
    PushBack(&words, NULL); // mark end
    is_previous_separator = 1;
    int index = 0;
    for (const char *ch = shell->current_command; *ch != 0; ++ch) {
        if (*ch == ' ') is_previous_separator = 1;
        else if (*ch == '|') {
            if (index == 0) {
                FreeArray(&words);

                printf("3230shell: should not have | before all command\n");
                shell->current_command_list = NewArray();
                return;
            } else if (*At(words, index - 1) == NULL) {
                // double pipe

                for (int i = 0; i < index; ++i)
                    if (*At(words, i))
                        free(*At(words, i));

                FreeArray(&words);

                printf("3230shell: should not have two consecutive | without in-between command\n");
                shell->current_command_list = NewArray();
                return;
            }

            is_previous_separator = 1;
            ++index;
        } else if (*ch == '&') { break; }
        else if (is_previous_separator) {
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
        shell->current_command_list = NewArray();
        return;
    }

    shell->current_command_list = words;
}

const char *GetProgramName(const char *path) {
    const char *exe_name = strrchr(path, '/');
    if (exe_name) ++exe_name;
    else exe_name = path;

    return exe_name;
}

/* -1 for leave it, -2 for close */
void runProgram(int in, int out, char **argv) {

    // redirect previous stdout to this input
    if (in == -2) {
        int nullFd[2];
        pipe(nullFd);
        dup2(nullFd[0], 0);
        close(nullFd[0]);
        // close(nullFd[1]);
    } else if (in != -1) {
        dup2(in, 0);
        close(in);
    }

    // redirect stdout to next input
    if (out == -2) {
        int nullFd[2];
        pipe(nullFd);
        dup2(nullFd[1], 1);
        close(nullFd[0]);
        close(nullFd[1]);
    } else if (out != -1) {
        dup2(out, 1);
        close(out);
    }

    // wait for SIGUSR1
    while (!g_sig1) pause();

    signal(SIGINT, SIG_DFL);

    execvp(*argv, argv);
    exit(errno);
}

void LogExitCode(int exit_code, char *process_name) {
    if (exit_code != 0) {
        switch (exit_code) {
            case 1: // simply program return 1
                break;
            case 2:
                printf("3230shell: '%s': No such file or directory\n", process_name);
                break;
            case 13:
                printf("3230shell: '%s': Permission denied\n", process_name);
                break;
            default:
                printf("3230shell: Unknown error code (%d)\n", exit_code);
        }
    }
}

void LogSignalled(int status) {
    if (!WIFEXITED(status)) {
        switch (WTERMSIG(status)) {
            case 1: // simply program return 1
                break;
            case SIGINT:
                printf("Interrupt\n");
                break;
            case SIGKILL:
                printf("Killed\n");
                break;
            case SIGTERM:
                printf("Terminated\n");
                break;
            default:
                printf("Unknown SIG code (%d)\n", WTERMSIG(status));
        }
    }
}

extern char **environ;

void FrontGroundJobTerminate(struct Shell *shell) {

    struct ProcessStats last_process = *(struct ProcessStats *) *At(shell->current_child_process_stats,
                                                                    Len(shell->current_child_process_stats) - 1);

    // exit code
    LogExitCode(WEXITSTATUS(last_process.status), last_process.name);

    // signalled
    LogSignalled(last_process.status);

    // stats
    if (shell->command_stats_required) {
        int commandPipeIndex = 0;
        for (int i = 0; i < Len(shell->current_child_process_stats); ++i) {

            last_process = *(struct ProcessStats *) *At(shell->current_child_process_stats, i);

            printf("(PID)%d  (CMD)%s    (user)%.3f s  (sys)%.3f s\n", last_process.pid, last_process.name,
                   last_process.rusageStats.ru_utime.tv_sec + last_process.rusageStats.ru_utime.tv_usec / 1000000.0,
                   last_process.rusageStats.ru_stime.tv_sec + last_process.rusageStats.ru_stime.tv_usec / 1000000.0);
        }
    }

    // clean child stats
    FreeCustomArrayElements(&shell->current_child_process_stats, FreeProcessStats);
    FreeArray(&shell->current_child_process_stats);
}

void StartChildProcess(struct Array chain) {
    for (int i = 0; i < Len(chain); ++i)
        kill(((struct ProcessStats *) *At(chain, i))->pid, SIGUSR1);
}

int ArgvSize(char **argv) {

    int size = 0;
    for (; argv[size]; ++size);
    return size;
}

char **AppendEnv(char **ori_argv, char **env) {

    int argv_size = ArgvSize(ori_argv);
    int env_size = ArgvSize(env);
    char **argv_env = malloc(sizeof(char *) * (argv_size + env_size + 2));

    for (int i = 0; i <= argv_size; ++i)
        argv_env[i] = ori_argv[i];
    for (int i = 0; i <= env_size; ++i)
        argv_env[argv_size + i] = env[i];

    return argv_env;
}

void runExec(struct Shell *shell, const struct Array words) {

    struct Array current_process_chain = NewArray();
    int childCount = CountNull(words);

    // new child process storage
    assert(Len(shell->current_child_process_stats) == 0);
    current_process_chain = NewArray();

    pid_t last_child;

    // background child should not read
    int current_pipe_in = shell->command_background_required ? -2 : -1;
    int commandPipeIndex = 0;

    int stdin_cpy = dup(0);
    int stdout_cpy = dup(1);

    // all child should wait for this signal
    signal(SIGUSR1, user_sig1_handler);

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
            FreeCustomArrayElements(&current_process_chain, FreeProcessStats);
            FreeArray(&current_process_chain);

            if (shell->command_background_required) {

                // no need to restore as not required
                setpgid(0, 0);
            }

            // putenv("TERM=vt100");

            runProgram(previous_pipe_in, pipefd[1], (char **) words.data + commandPipeIndex);
        }

        // close child's reading/writing pipe
        if (previous_pipe_in != -1) close(previous_pipe_in);
        if (pipefd[1] != -1) close(pipefd[1]);

        // ready to record the stats
        struct ProcessStats *finished_process = malloc(sizeof(struct ProcessStats));
        finished_process->pid = last_child;
        finished_process->status = -1;

        // copy program name
        const char *exe_name = GetProgramName(words.data[commandPipeIndex]);
        finished_process->name = malloc(sizeof(char) * (strlen(exe_name) + 1));
        strcpy(finished_process->name, exe_name);

        PushBack(&current_process_chain, finished_process);

        // advance to next pipe
        while (words.data[commandPipeIndex++]);

        // parent process keep creating
    }
    signal(SIGUSR1, SIG_DFL);

    if (!shell->command_background_required) {

        StartChildProcess(shell->current_child_process_stats = current_process_chain);

        // wait for all frontend job finish
        do {
            pause();
        } while (Len(shell->current_child_process_stats));
    } else {

        sigset_t child_mask;
        while ((sigemptyset(&child_mask) == -1) || (sigaddset(&child_mask, SIGCLD) == -1));
        if (sigprocmask(SIG_BLOCK, &child_mask, NULL) != -1) {

            struct Array *childChainSave = malloc(sizeof(struct Array));
            ShallowCopy(childChainSave, current_process_chain);

            PushBack(&shell->background_last_pipe_process_stats, childChainSave);
            while (sigprocmask(SIG_UNBLOCK, &child_mask, NULL) == -1);
        }

        StartChildProcess(current_process_chain);

        // save only the last process, free other
//        --current_process_chain.len;
//        FreeCustomArrayElements(&current_process_chain, FreeProcessStats);
//        FreeArray(&current_process_chain);
    }

    // restore stdin/out
    dup2(stdin_cpy, 0);
    dup2(stdout_cpy, 1);
}

void interpCommand(struct Shell *shell) {
    if (shell->current_command != NULL) {

        shell->command_stats_required = 0;
        splitCommand(shell);
        struct Array decorated_words = shell->current_command_list;

        // empty command
        if (Len(shell->current_command_list) == 0 ||
            *At(shell->current_command_list, 0) == NULL) {
            FreeArrayElements(&shell->current_command_list);
            FreeArray(&shell->current_command_list);
            return;
        }

        // default command

        // exit command
        if (strcmp((char*)*At(shell->current_command_list, 0), "exit") == 0) {

            if(shell->command_background_required){
                printf("3230shell: \"exit\" cannot be run in background mode\n");
                return;
            }

            if (Len(shell->current_command_list) == 2) {
                shell->running = 0;
                return;
            }
            else {
                printf("3230shell: \"exit\" with other arguments!!!\n");
                return;
            }
        }

        // timeX command
        if (strcmp((char *) *At(shell->current_command_list, 0), "timeX") == 0) {

            if (Len(shell->current_command_list) == 2) {
                printf("3230shell: \"timeX\" cannot be a standalone command\n");
                return;
            }

            if (shell->command_background_required) {
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

        FreeArrayElements(&shell->current_command_list);
        FreeArray(&shell->current_command_list);
    }
}

#endif