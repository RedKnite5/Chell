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
        
        /* Remove trailing newline from command line */
        nl = strchr(cmd, '\n');
        if (nl)
            *nl = '\0';
        
        /* Print command line if stdin is not provided by terminal */
        if (!isatty(STDIN_FILENO)) {
            printf("%s", cmd);
            fflush(stdout);
        }
        
        char *test;
        char *output = "";
        test = strtok(cmd, ">");
        if (test) {
            strcpy(cmd, test);
            output = strtok(NULL, " ");
            
        }
        
        printf("output: '%s'\n", output);
        
        
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
        
        //cmd = array[0];

        /* Builtin command */
        if (!strcmp(cmd, "exit")) {
            fprintf(stderr, "Bye...\n");
            printf("+ completed 'exit' [0]\n");
            break;
        } else if (!strcmp(cmd, "cd")) {
            //printf("Array[1]: '%s'\n", array[1]);
            chdir(array[1]);
            continue;
        }
        


        /* Regular command */
        int status;
        if (fork()) {
            waitpid(-1, &status, 0);
            
            retval = WEXITSTATUS(status);
            
            if (output) {
                freopen("/dev/tty", "w", stdout);
            }
        } else {
            if (output) {
                freopen(output, "w+", stdout); 
            }
            execvp(array[0], array);
            exit(1);
        }
        
        printf("+ completed '%s' [%d]\n", cmd, retval);
    }

    return EXIT_SUCCESS;
}