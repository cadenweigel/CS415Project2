#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_ARGS 100
#define MAX_CMDS 100

void trim_newline(char *str);
int read_input_file(const char *filename, char lines[][MAX_LINE]);
void parse_command(char *line, char **args);
pid_t spawn_process(char **args);
void wait_for_children(pid_t *pids, int count);

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char lines[MAX_CMDS][MAX_LINE]; //stores lines from input file
    char *args[MAX_ARGS]; //stores parsed arguments
    pid_t pids[MAX_CMDS]; //stores child process PIDs
    int line_count = read_input_file(filename, lines); //store inputs in lines and get count
    int pid_count = 0;

    for (int i = 0; i < line_count; i++) {
        if (strlen(lines[i]) == 0) continue; //skip if empty
        parse_command(lines[i], args);
        pids[pid_count++] = spawn_process(args);
    }

    wait_for_children(pids, pid_count); //wait for all child processes to finish
    printf("All processes have finished.\n");
    return 0;
}

void trim_newline(char *str) {
    //removes the newline character at the end of a string if it exists
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') str[len - 1] = '\0';
}

int read_input_file(const char *filename, char lines[][MAX_LINE]) {
    //reads input lines from the given file and stores them in the lines array
    //returns the number of lines, array is passed via reference so not returned
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    int count = 0;
    while (fgets(lines[count], MAX_LINE, file) != NULL && count < MAX_CMDS) {
        trim_newline(lines[count]);
        count++;
    }
    fclose(file);
    return count;
}

void parse_command(char *line, char **args) {
    //parses a single line into arguments suitable for execvp
    char *token = strtok(line, " ");
    int i = 0;
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;
}

pid_t spawn_process(char **args) {
    //forks and executes a command using execvp, returning the child process' PID
    pid_t pid = fork(); //create a new process
    if (pid < 0) { //if fork() returns -1 an error occurred
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        //child process executes command 
        printf("Child process PID %d executing: %s\n", getpid(), args[0]);
        execvp(args[0], args); //replace the current process image with the new program
        perror("execvp"); //if execvp returns, it failed
        exit(EXIT_FAILURE);
    }
    return pid; //parent process returns child process PID
}

void wait_for_children(pid_t *pids, int count) {
    //waits for all child processes to terminate
    for (int i = 0; i < count; i++) {
        waitpid(pids[i], NULL, 0);
    }
}
