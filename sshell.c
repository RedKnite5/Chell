#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define CMDLINE_MAX 512

void split_str(char *string, char *array[16]) {

    array[0] = strtok(string, " ");
    
    int arg = 1;
    while(array[arg] != NULL) {
        array[arg] = strtok(NULL, " ");
        arg += 1;
    }
}

int main(void)
{
    char cmd[CMDLINE_MAX];

    while (1) {
        char *nl;
        int retval;

        /* Print prompt */
        printf("sshell@ucd$ ");
        fflush(stdout);

        /* Get command line */
        fgets(cmd, CMDLINE_MAX, stdin);

        /* Print command line if stdin is not provided by terminal */
        if (!isatty(STDIN_FILENO)) {
            printf("%s", cmd);
            fflush(stdout);
        }

        /* Remove trailing newline from command line */
        nl = strchr(cmd, '\n');
        if (nl)
            *nl = '\0';

        /* Builtin command */
        if (!strcmp(cmd, "exit")) {
            fprintf(stderr, "Bye...\n");
            printf("+ completed 'exit' [0]\n");
            break;
        }

        /* Regular command */
        char *array[16];
        char *token;
        token = strtok(cmd, " ");
        array[0] = token;
    
        int arg = 1;
        while(token != NULL) {
            token = strtok(NULL, " ");
            array[arg] = token;
            arg += 1;
        }
        
        int status;
        if (fork()) {
            waitpid(-1, &status, 0);
            
            retval = WEXITSTATUS(status);
        } else {
            execvp(array[0], array);
            exit(1);
        }
        
        printf("+ completed '%s' [%d]\n", cmd, retval);
    }

    return EXIT_SUCCESS;
}