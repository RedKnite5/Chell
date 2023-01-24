#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdbool.h>  // booleans
#include <ctype.h>



#define CMDLINE_MAX 512

// got this from https://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
// len is size of output buffer. Not string
size_t trimwhitespace(char *out, size_t len, const char *str) {
    if (len == 0)
        return 0;

    const char *end;
    size_t out_size;

    // Trim leading space
    while (isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == 0) {  // All spaces?
        *out = 0;
        return 1;
    }

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    end++;
    //printf("start: '%d'\n", str);
    //printf("end: '%s'\n", end);

    // Set output size to minimum of trimmed string length and buffer size minus 1
    if ((end - str) < (long int)len-1) {
        out_size = end - str;
        //printf("out_size: '%d'\n", out_size);
    } else {
        out_size = len-1;
    }
    //out_size = (end - str) < (long int)len-1 ? (size_t)(end - str) : len-1;

    // Copy trimmed string and add null terminator
    memcpy(out, str, out_size);
    out[out_size] = 0;

    return out_size;
}


size_t split_string(char **array, const char *str, const char *split) {
    char str_copy[CMDLINE_MAX];
    memcpy(str_copy, str, CMDLINE_MAX);

    char *token = strtok(str_copy, split);
    char stripped[CMDLINE_MAX] = "";
    
    size_t arg = 0;
    while (strlen(token) > 0) {
        trimwhitespace(stripped, CMDLINE_MAX, token);
        /*array[arg] = stripped;*/

        array[arg] = (char *)malloc(sizeof(char) * (strlen(stripped) + 1));
        strcpy(array[arg], stripped);

        arg += 1;
        token = strtok(NULL, split);
        if (token == NULL) {
            break;
        }
    } 

    array[arg] = NULL;

    /*
    char* ptr = strchr(str, *split);  // doesnt support multi char splits
    if (ptr != NULL) {
        ptr = ptr + 1;
        *str = *ptr;
    }
    */

    //printf("\n");
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
    return 'x';
}

int run_commands(char *cmd, bool flag) {
    //fprintf(stderr, "Run command: %s\n", cmd);
    int retval;
    
    /* Split command on redirection operator */
    char *output = NULL;
    char *redirection[3];
    char mode = parse_redirection(redirection, cmd);
    strcpy(cmd, redirection[0]);

    if (redirection[1] != NULL) {
        output = redirection[1];
    }
    

    /* Split command into arguments */
    char *array[16];
    split_string(array, cmd, " ");
    strcpy(cmd, array[0]);

    /* Note that redirection elements may not print, because the redirection array is seperate from the command array*/
    //for (int i=0; i<b; i++) {
    //    fprintf(stderr, "array %d: '%s'\n", i, array[i]);
    //}
    

    /* Builtin commands */
    if (!strcmp(array[0], "exit")) {
        fprintf(stderr, "Bye...\n");
        fprintf(stderr, "+ completed 'exit' [0]\n");
        exit(EXIT_SUCCESS);
    } else if (!strcmp(array[0], "cd")) {
        int retval = chdir(array[1]);
        fprintf(stderr, "+ completed 'cd' [%d]\n", retval);
        return retval;
    }
    

    /* Regular command */
    int status;

    if (!flag) {
        if (fork()) {
            waitpid(-1, &status, 0);
        
            retval = WEXITSTATUS(status);
        } else {
            if (output != NULL) {
                if (mode == 'w') {
                    freopen(output, "w+", stdout);
                } else {
                    freopen(output, "a+", stdout);
                }
            }

            //fprintf(stderr, "!Flag Execution %d: '%s'\n", 0, array[0]);
            execvp(array[0], array);

            exit(1);
        }
    } else {
        //fprintf(stderr, "+Flag Execution %d: '%s'\n", 0, array[0]);
        execvp(array[0], array);
        exit(1);
    }

    return retval;
}


int main(void) {
    
    char cmd[CMDLINE_MAX];

    while (1) {
        int retval;
        char *newLine;

        /* Shell prompt */
        printf("sshell@ucd$ ");
        fflush(stdout);

        /* Get User Input */
        fgets(cmd, CMDLINE_MAX, stdin);
        
        /* Remove trailing newline */
        newLine = strchr(cmd, '\n');
        if (newLine) {
            *newLine = '\0';
        }
        
        /* Print command line if stdin is not provided by terminal */
        if (!isatty(STDIN_FILENO)) {
            printf("%s", cmd);
            fflush(stdout);
        }
        
        if (!strcmp(cmd, "")) {
            continue;
        }
        
        /* Tokenize arguments using pipe character as delimiter (At most 3 pipe ops) */
        char *pipe_commands[3];
        size_t arg = split_string(pipe_commands, cmd, "|");
        
        int NUM_PIPES = arg;
        
        /* Pipe Commands Present*/
        if (arg > 1) {
            pid_t pid;
            int mypipes[NUM_PIPES][2];

            for (int i = 0; i < NUM_PIPES-1; i++) {
                if(pipe(mypipes[i])) {
                    fprintf(stderr, "Pipe %d failed.\n", i);
                    return EXIT_FAILURE;
                }
            }

            /* Create child processes*/
            for(int i = NUM_PIPES-1; i >= 0; i--) {
                pid = fork();
                if (pid == (pid_t) 0) {
                    /* Child Process, Close Parent Pipe*/

                    if (arg > 2) {
                        if (i == 0) {
                            close(mypipes[0][0]);
                            dup2(mypipes[0][1], STDOUT_FILENO);    /* WRITE TO NEXT COMMANDS INPUT */
                        } else if (i < NUM_PIPES-1) {
                            close(mypipes[i-1][1]);
                            dup2(mypipes[i-1][0], STDIN_FILENO);  /* READ FROM PREVIOUS COMMANDS OUTPUT */
                            close(mypipes[i][0]);
                            dup2(mypipes[i][1], STDOUT_FILENO);   /* WRITE TO NEXT COMMANDS INPUT */
                        } else if (i == NUM_PIPES-1) {
                            close(mypipes[i-1][1]);
                            dup2(mypipes[i-1][0], STDIN_FILENO);    /* READ FROM PREVIOUS COMMANDS OUTPUT */
                        }
                    } else {
                        if (i == 0) {
                            close(mypipes[0][0]);
                            dup2(mypipes[0][1], STDOUT_FILENO);
                        } else {
                            close(mypipes[0][1]);
                            dup2(mypipes[0][0], STDIN_FILENO);
                        }
                    }

                    run_commands(pipe_commands[i], true);
                    exit(EXIT_FAILURE);
                } else if (pid < (pid_t) 0) {
                    fprintf(stderr, "Fork %d failed.\n", i);
                    exit(EXIT_FAILURE);
                } else {
                    /* This is the parent process. Close both sides of pipe. */
                    close(mypipes[i][0]);
                    close(mypipes[i][1]);
                    
                }
            }
        } else {  /* No Pipe Commands*/
            printf("3\n");
            retval = run_commands(pipe_commands[0], false);
        }
        
        fprintf(stderr, "+ completed '%s' [%d]\n", cmd, retval);
    }

    return EXIT_SUCCESS;
}