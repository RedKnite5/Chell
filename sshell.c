#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>



#define CMDLINE_MAX 512


size_t string_len(const char *str) {
    size_t count = 0;
    for (count=0; str[count] != '\0'; count++) {}
    return count;
}

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
    printf("start: '%d'\n", str);
    printf("end: '%s'\n", end);

    // Set output size to minimum of trimmed string length and buffer size minus 1
    if ((end - str) < (long int)len-1) {
        out_size = end - str + 1;
        printf("out_size: '%d'\n", out_size);
    } else {
        out_size = len-1;
    }
    //out_size = (end - str) < (long int)len-1 ? (size_t)(end - str) : len-1;

    // Copy trimmed string and add null terminator
    memcpy(out, str, out_size);
    out[out_size] = '\0';

    return out_size;
}


size_t split_string(char **array, char *str, char *split) {
    char save[CMDLINE_MAX] = "";
    printf("input: '%s'\n", str);
    printf("split: '%s'\n", split);
    //strcpy(save, str);
    //printf("save: '%s'\n", save);
    char *token = strtok(str, ">");
    char stripped[CMDLINE_MAX] = "";
    printf("token: '%s'\n", token);
    printf("token bool: '%d'\n", token == NULL);
    printf("token_content: '%d'\n", *token);
    
    if (token == NULL) {
        //printf("str: '%s'\n", save);
        array[0] = str;
        array[1] = NULL;
        return 1;
    }
    
    size_t arg = 0;
    while (token != NULL) {
        trimwhitespace(stripped, CMDLINE_MAX, token);
        printf("stripped: '%s'\n", stripped);
        array[arg] = stripped;
        token = strtok(NULL, split);
        arg += 1;
    }
    array[arg] = NULL;
    return arg;
}

int run_commands(char *cmd) {
    int retval;
    printf("cmd: '%s'\n\n", cmd);
    
    /* Split command on redirection operator */
    char *output = NULL;
    char *redirection[3];
    split_string(redirection, cmd, ">");
    printf("\nafter split: '%s'\n", redirection[0]);
    strcpy(cmd, redirection[0]);
    if (redirection[1] != NULL) {
        output = redirection[1];
    }
    
    
    /* Split command into arguments */
    char *array[16];
    split_string(array, cmd, " ");
    strcpy(cmd, array[0]);
    
    /* Builtin command */
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
    if (fork()) {
        waitpid(-1, &status, 0);
        
        retval = WEXITSTATUS(status);
        
        //if (output != NULL) {
        //    freopen("/dev/tty", "w", stdout);
        //}
    } else {
        if (output != NULL) {
            freopen(output, "w+", stdout); 
        }
        execvp(array[0], array);
        exit(1);
    }
    return retval;
}


int main(void) {
    /*
    char *array[50];
    char *array2[50];
    char *str = "ls";
    split_string(array, str, ">");
    split_string(array2, array[0], ">");
    
    return 0;
    */
    
    /*
    char array[50];
    char *array2[50];
    char *str = "ls";
    trimwhitespace(array, 50, str);
    split_string(array2, array, ">");
    printf("array2: '%s'\n", array2[0]);
    
    return 0;
    */
    
    //char *str = "ls";
    //char *token = strtok(str, ">");
    //printf("token: '%s'\n", token);
    
    //return 0;
    
    
    
    char cmd[CMDLINE_MAX];

    while (1) {
        char *nl;
        int retval;

        /* Print prompt */
        printf("sshell@ucd$ ");
        fflush(stdout);

        /* Get command line */
        fgets(cmd, CMDLINE_MAX, stdin);
        
        /* Remove trailing newline from command line */
        nl = strchr(cmd, '\n');
        if (nl)
            *nl = '\0';
        
        /* Print command line if stdin is not provided by terminal */
        if (!isatty(STDIN_FILENO)) {
            printf("%s", cmd);
            fflush(stdout);
        }
        
        /* Split command on pipe operator */
        char *pipe_commands[5];
        size_t arg = split_string(pipe_commands, cmd, "|");
        
        for (size_t i = arg; i > 0; i--) {
            if (i > 1) {
                // piping
                
                pid_t pid;
                int mypipe[2];
                
                if (pipe(mypipe)) {
                    exit(EXIT_FAILURE);
                }
                
                pid = fork();
                if (pid == (pid_t) 0) {
                    /* This is the child process.
                     Close other end first. */
                    close(mypipe[1]);
                    
                    dup2(mypipe[0], STDIN_FILENO);
                    
                    if (i > 2) {
                        continue;
                    } else {
                        retval = run_commands(pipe_commands[i-1]);
                        break;
                    }
                } else if (pid < (pid_t) 0) {
                    /* The fork failed. */
                    fprintf(stderr, "Fork failed.\n");
                    retval = EXIT_FAILURE;
                } else {
                    /* This is the parent process.
                     Close other end first. */
                    close (mypipe[0]);
                    
                    dup2(mypipe[1], STDOUT_FILENO);
                    run_commands(pipe_commands[i-1]);
                }
            } else {
                retval = run_commands(pipe_commands[0]);
            }
        }
        
        fprintf(stderr, "+ completed '%s' [%d]\n", cmd, retval);
    }

    return EXIT_SUCCESS;
}