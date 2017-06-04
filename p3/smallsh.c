#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>

/* maximum number of shell arguments supported */
#define MAX_ARG 512

/* maximum number of characters per line of input */
#define MAX_IN 2048

/* reflects manner in which last foreground process ended.  NONE implies
 * no last last foreground process exists */
enum stat_type { NONE, TERM, EXIT };


/* functions */
int bg_check(pid_t **bg_pids, int *num_bg, int max_bg);
void cd(char *dir);
void clean_up(pid_t *bg_pids, int n);
void read_tokens(char **tokens, char **args, char **cmd, 
                 char **redir_in, char **redir_out, int *bg);
void status(int cpid, enum stat_type stat, int status);


/* Checks on each background process started by shell and possibly
 * still running.  Processes still running are stored in *bg_pids. */
int bg_check(pid_t **bg_pids, int *num_bg, int max_bg)
{
    /* list of pids still running */
    pid_t *running_pids;
    int i, j = 0;
    int status, cur_pid;

    /* if no background processes, nothing to do */
    if (!(*num_bg))
        return 1;

    if (!(running_pids = malloc(max_bg * sizeof(pid_t)))) {
        perror("could not allocate memory");
        return 0;
    }

    /* check to see if each background process has exited or terminated */
    for (i = 0; i != *num_bg; ++i) {
        cur_pid = waitpid((*bg_pids)[i], &status, WNOHANG);
        /* if the current process has finished, print status */
        if (cur_pid > 0) {
            if (WIFEXITED(status))
                printf("background pid %d is done: exit value %d\n",
                        cur_pid, WEXITSTATUS(status));
            else if (WIFSIGNALED(status))
                printf("background pid %d is done: terminated by signal %d\n",
                        cur_pid, WTERMSIG(status));
            
        }
        /* otherwise add it to the list of running background processes */
        else 
            running_pids[j++] = (*bg_pids)[i];
    }

    *num_bg = j;
    free(*bg_pids);
    *bg_pids = running_pids;
    return 1;
}


/* Changes the current working directory.  If dir is null, changes to
   to user's home directory. */
void cd(char *dir) {
    /* if a directory name was given, attempt to change to it */
    if (dir) {
        if (chdir(dir) == -1)
            perror("cd: could not change to directory");
    }
    /* otherwise mimic normal cd behavior and change to user's home */
    else 
        chdir(getenv("HOME"));
}


/* Sends kill signal to each background process still running */
void clean_up(pid_t *bg_pids, int n)
{
    for (--n; n >= 0; --n)
        kill(bg_pids[n], SIGKILL);
}


/* Parses tokens for a command cmd, its arguments args, and
 * any I/O redirections.  If redir_in is null, input to cmd
 * is stdin.  If redir_out is null, cmd outputs to stdout.  If
 * *bg is 0, cmd is meant to run as a foreground process. */
void read_tokens(char **tokens, char **args, char **cmd, 
                 char **redir_in, char **redir_out, int *bg)
{
    int tpos = 0, apos = 0;
    *bg = 0;
    args[0] = args[1] = *cmd = *redir_in = *redir_out = NULL;

    /* if first token isn't null, it's the command */
    if (*cmd = tokens[tpos++]) {
        /* for exec(), set first arg to command name */
        args[apos++] = *cmd;

        /* parse tokens for redirection and background symbols */
        while (tokens[tpos]) {
            /* if first "<", redirect input from next token */
            if (!*redir_in && (strcmp(tokens[tpos], "<") == 0)) {
                *redir_in = tokens[++tpos];
            }
            /* if first ">", redirect output to next token */
            else if (!*redir_out && (strcmp(tokens[tpos], ">") == 0)) {
                *redir_out = tokens[++tpos];
            }
            /* if "&", set background flag and stop parsing tokens */
            else if (strcmp(tokens[tpos], "&") == 0) {
                *bg = 1;
                break;
            }
            /* otherwise store the token as an argument */
            else {
                args[apos++] = tokens[tpos];
            }
            ++tpos;
        }
        /* signal end of arguments */
        args[apos] = NULL;
    } 
}


/* Prints information about the completion of the last foreground process
   initiated by the shell, if one exists. */
void status(int cpid, enum stat_type stat, int status)
{
    /* check whether last foreground command was exited normally
       or was terminated and print status */
    switch (stat) {
        case EXIT:
            printf("foreground pid %d is done: exit value %d\n",
                    cpid, status);
            break;
        case TERM:
            printf("foreground pid %d is done: terminated by signal %d\n",
                    cpid, status);
            break;
        /* otherwise there wasn't a last foreground command */
        default:
            printf("all good in the smallsh hood\n");
            break;
    }
}


int main(int argc, char *argv[])
{
    pid_t last_fg, *bg_pids;
    enum stat_type stat = NONE;
    int child_status;
    int num_bg = 0;
    int max_bg = 10;
    
    if (!(bg_pids = malloc(max_bg * sizeof(pid_t)))) {
        perror("could not allocate memory");
        return 1;
    }

    /* ignore interrupt signal */
    signal(SIGINT, SIG_IGN);

    /* prompt loop */
    while (1) {
        char line[MAX_IN];
        char **tokens, **args;
        char *cmd = NULL, *redir_in = NULL, *redir_out = NULL;
        int bg = 0, pos = 0, fd_in, fd_out;
        pid_t child;

        /* leave room for redirection symbols, command, and null
           pointer at end */
        if (!(tokens = malloc((MAX_ARG + 4) * sizeof(char *)))) {
            perror("could not allocate memory for tokens");
            free(bg_pids);
            return 1;
        }

        /* leave room for command name at beginning of array and
           null pointer at end */
        if (!(args = malloc((MAX_ARG + 2) * sizeof(char *)))) {
            perror("could not allocate memory for arguments");
            free(bg_pids);
            free(tokens);
            return 1;
        }

        /* print information about any background process that has
           exited or been terminated */
        if (!bg_check(&bg_pids, &num_bg, max_bg)) {
            clean_up(bg_pids, num_bg);
            free(bg_pids);
            free(tokens);
            free(args);
            return 1;
        }

        /* prompt for command line */
        printf(": ");
        fflush(stdout);

        /* read user input line, supports up to MAX_IN chars */
        fflush(stdin);
        fgets(line, MAX_IN, stdin);

        /* grab all tokens from user input */
        tokens[pos++] = strtok(line, " \n");
        while (tokens[pos++] = strtok(NULL, " \n"));
        
        /* interpret the tokens to find out command name, arguments list,
           whether I/O is redirected, and whether command is to run in
           foreground or background */
        read_tokens(tokens, args, &cmd, &redir_in, &redir_out, &bg);

        /* if no command given, nothing to do */
        if (!cmd)
            continue;

        /* exit builtin: kill background commands and deallocate memory
           before exiting successfully */
        if (strcmp(cmd, "exit") == 0) {
            clean_up(bg_pids, num_bg);
            free(bg_pids);
            free(tokens);
            free(args);
            exit(0);
        }
        /* cd builtin */
        else if (strcmp(cmd, "cd") == 0)
            cd(args[1]);
        /* status builtin */
        else if (strcmp(cmd, "status") == 0)
            status(last_fg, stat, child_status);
        /* otherwise, if user didn't enter a comment line, fork a new
           process and attempt to run user's command */
        else if (cmd[0] != '#') {
            child = fork();

            switch (child) {
                case -1: {
                    perror("fork failed");
                    break;
                }
                /* inside child */
                case 0: {
                    /* disconnect background process from stdin and stdout */
                    if (bg) {
                        if (!redir_in)
                            redir_in = "/dev/null";
                        if (!redir_out)
                            redir_out = "/dev/null";
                    } 
                    /* don't let foreground process ignore interrupt signal */
                    else {
                        signal(SIGINT, SIG_DFL);
                    }

                    /* open file and redirect input from it */
                    if (redir_in) {
                        fd_in = open(redir_in, O_RDONLY);
                        if (fd_in != -1)
                            fd_in = dup2(fd_in, 0);
                        else {
                            printf("cannot open %s for input\n", redir_in);
                            free(tokens);
                            free(args);
                            exit(1);
                        }
                    } 
                    /* create or truncate file and redirect output to it */
                    if (redir_out) {
                        fd_out = open(redir_out, O_WRONLY | O_CREAT | O_TRUNC, 0664);
                        if (fd_out != -1)
                            fd_out = dup2(fd_out, 1);
                        else {
                            printf("cannot open %s for output\n", redir_out);
                            free(tokens);
                            free(args);
                            exit(1);
                        }

                    }

                    /* attempt to run user's command, passing it args */
                    execvp(cmd, args);
                    printf("%s: no such file or directory\n", cmd);
                    free(tokens);
                    free(args);
                    exit(1);
                }
                /* inside parent/shell */
                default: {
                    /* if background process, add its pid to the list
                       to check later and continue prompting */
                    if (bg) {
                        if (num_bg == max_bg) {
                            max_bg += 10;
                            if (!realloc(bg_pids, max_bg * sizeof(pid_t))) {
                                perror("could not reallocate memory for bg_pids");                          clean_up(bg_pids, num_bg);
                                free(bg_pids);
                                free(tokens);
                                free(args);
                                exit(1);
                            }
                        }
                        bg_pids[num_bg++] = child;
                        printf("background pid is %d\n", child);
                        fflush(stdout);
                    }
                    /* if foreground process, don't return to prompt
                       until child exits or is terminated */
                    else {
                        /* wait for foreground process to finish and
                           store information when it does */
                        last_fg = waitpid(child, &child_status, 0);
                        if (last_fg > 0) {
                            if (WIFSIGNALED(child_status)) {
                                child_status = WTERMSIG(child_status);
                                stat = TERM;
                                printf("terminated by signal %d\n",
                                        child_status);
                                fflush(stdout);
                            } else if (WIFEXITED(child_status)) {
                                child_status = WEXITSTATUS(child_status);
                                stat = EXIT;
                            } else {
                                stat = NONE;
                            }
                        }
                    }
                    break;
                }
            }
        }
        free(tokens);
        free(args);
    }
    return 1;
}
