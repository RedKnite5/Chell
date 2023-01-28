#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdbool.h> // Header-file for boolean data-type.
#include <ctype.h>
#include <errno.h>

#define CMDLINE_MAX 512
#define MAX_PIPES 5
#define MAX_ARGS 16
#define UNLIKELY_RETVAL 25


struct Job {
    char cmd[CMDLINE_MAX];
    pid_t pid;
};

struct Node {
	struct Job data;
	struct Node *next;
};

void push(struct Node **head, struct Job data) {
	struct Node* new_node = (struct Node*) malloc(sizeof(struct Node));

	new_node->data = data;
	new_node->next = *head;

	(*head) = new_node;
}

void delete(struct Node **head, int pid) {
    if (head == NULL) {
        return;
    }

    struct Node *prev = NULL;
    struct Node *current = *head;
    while (current->data.pid != pid) {
        if (current->next == NULL) {
            return;
        }
        prev = current;
        current = current->next;
    }

    if (current == *head) {
        *head = (*head)->next;
        return;
    }

    prev->next = current->next;
}

// inspired by https://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
// len is size of output buffer. Not string
size_t trimwhitespace(char *out, size_t len, const char *str) {
    if (len == 0)
        return 0;

    // Move str to first non whitespace char
    while (isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == 0) {  // All spaces?
        *out = 0;
        return 1;
    }

    const char *end;
    size_t out_size;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    end++;

    // Set output size to minimum of trimmed string length and buffer size minus 1
    if ((end - str) < (long int)len-1) {
        out_size = end - str;
    } else {
        out_size = len-1;
    }

    // Copy trimmed string and add null terminator
    memcpy(out, str, out_size);
    out[out_size] = '\0';

    return out_size;
}

bool background_check(char *cmd, int *error) {
    char *amper = strchr(cmd, '&');
    if (amper == NULL) {
        return true;
    } else if (*(amper+1) == '\0') {
        *amper = '\0';
        return false;
    } else {
        fprintf(stderr, "Error: mislocated background sign\n"); // Found Ampersand not at the end
        *error = 1;
        return false;  // doesn't matter true or false
    }
}

size_t split_string(char **array, const char *str, char *split) {
    char str_copy[CMDLINE_MAX];
    memcpy(str_copy, str, CMDLINE_MAX);

    char *token = strtok(str_copy, split);
    char stripped[CMDLINE_MAX] = "";

    size_t arg = 0;
    while (strlen(token) > 0) {
        trimwhitespace(stripped, CMDLINE_MAX, token);

        array[arg] = (char *)malloc(sizeof(char) * (strlen(stripped) + 1));
        strcpy(array[arg], stripped);

        arg += 1;
        token = strtok(NULL, split);
        if (token == NULL) {
            break;
        }
    }
    array[arg] = NULL;
    return arg;
}

char parse_redirection(char **output, const char *cmd) {
    char str_copy[CMDLINE_MAX];
    memcpy(str_copy, cmd, CMDLINE_MAX);

    split_string(output, cmd, ">>");
    if (output != NULL) {
        return 'a';
    }

    split_string(output, cmd, ">");
    if (output != NULL) {
        return 'w';
    }
    return 'x';  // not used
}

void setup_pipes(pid_t (*mypipes)[2], int i, int NUM_PIPES) {
    if (NUM_PIPES > 1) {
        if (i == 0) {
            close(mypipes[0][0]);
            dup2(mypipes[0][1], STDOUT_FILENO);    /* WRITE TO NEXT COMMANDS INPUT */
            close(mypipes[0][1]);
        } else if (i < NUM_PIPES) {
            close(mypipes[i-1][1]);
            dup2(mypipes[i-1][0], STDIN_FILENO);  /* READ FROM PREVIOUS COMMANDS OUTPUT */
            close(mypipes[i-1][0]);
            close(mypipes[i][0]);
            dup2(mypipes[i][1], STDOUT_FILENO);   /* WRITE TO NEXT COMMANDS INPUT */
            close(mypipes[i][1]);
        } else if (i == NUM_PIPES) {
            close(mypipes[i-1][1]);
            dup2(mypipes[i-1][0], STDIN_FILENO);    /* READ FROM PREVIOUS COMMANDS OUTPUT */
            close(mypipes[i-1][0]);
        }
    } else {
        if (i == 0) {
            close(mypipes[0][0]);
            dup2(mypipes[0][1], STDOUT_FILENO);
            close(mypipes[0][1]);
        } else {
            close(mypipes[0][1]);
            dup2(mypipes[0][0], STDIN_FILENO);
            close(mypipes[0][0]);
        }
    }
}

void file_redirection(char *file, char mode) {
    if (file != NULL) {
        FILE *success;
        if (mode == 'w') {
            success = freopen(file, "w+", stdout);
        } else {
            success = freopen(file, "a+", stdout);
        }
        if (success == NULL) {
            exit(UNLIKELY_RETVAL);
        }
    }
}

int run_commands(
    char *cmd,
    bool piping,
    bool wait,
    pid_t *background_pid,
    int *error,
    bool running_jobs
) {
    int retval;

    char stripped[CMDLINE_MAX];
    trimwhitespace(stripped, CMDLINE_MAX, cmd);
    if (*stripped == '>') {
        fprintf(stderr, "Error: missing command\n");
        *error = 1;
        return 1;
    }
    char *end = strchr(stripped, '\0')-1;
    if (*end == '>') {
        fprintf(stderr, "Error: no output file\n");
        *error = 1;
        return 1;
    }

    char *output = NULL;
    char *redirection[3];
    char mode = parse_redirection(redirection, cmd);
    strcpy(cmd, redirection[0]);

    if (redirection[1] != NULL) {
        output = redirection[1];
    }

    /* Split command into arguments */
    char *array[CMDLINE_MAX];
    int args = split_string(array, cmd, " ");
    strcpy(cmd, array[0]);

    if (args > MAX_ARGS) {
        fprintf(stderr, "Error: too many process arguments\n");
        *error = 1;
        return 1;
    }


    /* Builtin commands */
    if (!strcmp(array[0], "exit")) {
        if (!running_jobs) {
            fprintf(stderr, "Bye...\n");
            fprintf(stderr, "+ completed 'exit' [0]\n");
            exit(EXIT_SUCCESS);
        }
        fprintf(stderr, "Error: active jobs still running\n");
        return 1;
    } else if (!strcmp(array[0], "cd")) {
        int retval = chdir(array[1]);
        if (retval == -1) {
            fprintf(stderr, "Error: cannot cd into directory\n");
            return 1;
        }
        return 0;
    }


    /* Regular command */
    int status;
    if (!piping) {
        int pid = fork();
        *background_pid = pid;
        if (pid) {
            if (wait) {
                waitpid(pid, &status, 0);
                retval = WEXITSTATUS(status);
                if (retval == UNLIKELY_RETVAL) {
                    fprintf(stderr, "Error: cannot open output file\n");
                    *error = 1;
                    return 1;
                }
            }
        } else {
            file_redirection(output, mode);

            execvp(array[0], array);
            fprintf(stderr, "Error: command not found\n");
            exit(1);
        }
    } else {
        file_redirection(output, mode);

        execvp(array[0], array);
        fprintf(stderr, "Error: command not found\n");
        exit(1);
    }
    return retval;
}


int main(void) {
    struct Node *jobs = NULL;
    char cmd[CMDLINE_MAX];
    while (1) {
        int retval;
        char *newLine;
        bool wait = true;

        /* Shell prompt */
        printf("sshell@ucd$ ");
        fflush(stdout);

        /* Get User Input */
        fgets(cmd, CMDLINE_MAX, stdin);

        /* Print command line if stdin is not provided by terminal */
        if (!isatty(STDIN_FILENO)) {
            printf("%s", cmd);
            fflush(stdout);
        }

        /* Remove trailing newline */
        newLine = strchr(cmd, '\n');
        if (newLine) {
            *newLine = '\0';
        }

        if (!strcmp(cmd, "")) {
            continue;
        }

        struct Job job;
        strcpy(job.cmd, cmd);

        int error = 0;
        wait = background_check(cmd, &error);
        if (error) {
            continue;
        }

        pid_t background_pid;

        char stripped[CMDLINE_MAX];
        trimwhitespace(stripped, CMDLINE_MAX, cmd);
        char *end = strchr(stripped, '\0')-1;
        if (*stripped == '|' || *end == '|') {
            fprintf(stderr, "Error: missing command\n");
            continue;
        }

        /* Tokenize arguments using pipe character as delimiter
        (At most 3 pipe ops) */
        char *pipe_commands[MAX_PIPES];
        size_t arg = split_string(pipe_commands, cmd, "|");

        int NUM_PIPES = arg-1;

        pid_t pipe_pids[MAX_PIPES] = {0, 0, 0, 0, 0};
        pid_t pipe_pid;  // not used

        /* Pipe Commands Present*/
        if (arg > 1) {
            pid_t pid;
            int mypipes[arg][2];

            for (int i=0; i<NUM_PIPES; i++) {
                char *arrow = strchr(pipe_commands[i], '>');
                if (arrow != NULL) {
                    fprintf(stderr, "Error: mislocated output redirection\n");
                    continue;
                }
            }

            /* Create child processes*/
            for(int i = NUM_PIPES; i >= 0; i--) {
                if (i > 0) {
                    if(pipe(mypipes[i-1])) {
                        fprintf(stderr, "Pipe %d failed.\n", i-1);
                        return EXIT_FAILURE;
                    }
                }
                pid = fork();
                if (pid == (pid_t) 0) {
                    /* This is the child process */
                    setup_pipes(mypipes, i, NUM_PIPES);

                    int error = 0;
                    run_commands(pipe_commands[i], true, wait, &pipe_pid, &error, jobs!=NULL);
                    fprintf(stderr, "failed %s", pipe_commands[i]);
                    exit(EXIT_FAILURE);
                } else if (pid < (pid_t) 0) {
                    fprintf(stderr, "Fork %d failed.\n", i);
                    exit(EXIT_FAILURE);
                } else {
                    /* This is the parent process */
                    pipe_pids[i] = pid;
                    close(mypipes[i][0]);
                    close(mypipes[i][1]);
                }
            }
        } else {  /* No Pipe Commands*/
            int error = 0;
            retval = run_commands(pipe_commands[0], false, wait, &background_pid, &error, jobs!=NULL);
            if (error) {
                continue;
            }
        }

        if (!wait) {
            job.pid = background_pid;
            push(&jobs, job);
        }

        if (wait && arg > 1) {
            for (size_t i=0; i<arg; i++) {
                waitpid(pipe_pids[i], &retval, 0);
            }
        }

        int job_status;
        pid_t return_pid ;
        struct Node *pids = jobs;
        while (pids != NULL) {
            return_pid = waitpid(pids->data.pid, &job_status, WNOHANG);
            if (return_pid == pids->data.pid) {
                fprintf(stderr, "+ completed '%s' [%d]\n", pids->data.cmd, job_status);
                delete(&jobs, pids->data.pid);
            }
            pids = pids->next;
        }

        if (wait) {
            fprintf(stderr, "+ completed '%s' [%d]\n", cmd, retval);
        }
    }

    return EXIT_SUCCESS;
}