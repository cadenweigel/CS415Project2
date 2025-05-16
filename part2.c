#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_ARGS 100
#define MAX_CMDS 100

//created for part1
void trim_newline(char *str);
int read_input_file(const char *filename, char lines[][MAX_LINE]);
void parse_command(char *line, char **args);
pid_t spawn_process(char **args);
void wait_for_children(pid_t *pids, int count);

//created for part2
void setup_sigusr1_blocking(sigset_t *sigset); //sets up signal blocking so child processes can wait for SIGUSR1
pid_t fork_child_process(char *line, sigset_t *sigset); //forks a new child process and has it wait for SIGUSR1 before calling execvp
void signal_children(pid_t *pids, int count); //sends SIGUSR1, SIGSTOP, and SIGCONT to all child processes

int main() {
    char lines[MAX_CMDS][MAX_LINE]; //stores lines read from input.txt
    pid_t pids[MAX_CMDS];           //stores child process PIDs
    sigset_t sigset;                //signal set for SIGUSR1

    setup_sigusr1_blocking(&sigset); //block SIGUSR1 so that child processes can wait on it using sigwait()

    int line_count = read_input_file("input.txt", lines); //store inputs in lines and get count
    int pid_count = 0;

    //fork a child process for each command
    for (int i = 0; i < line_count; i++) {
        if (strlen(lines[i]) == 0) continue; //kip empty lines
        pids[pid_count++] = fork_child_process(lines[i], &sigset);
    }

    signal_children(pids, pid_count); //signal the child processes to start and control their execution
    wait_for_children(pids, pid_count); //wait for all child processes to terminate
    printf("MCP: All processes have finished.\n");

    return 0;
}

void setup_sigusr1_blocking(sigset_t *sigset) {
    //initialize signal set and add SIGUSR1
    sigemptyset(sigset);
    sigaddset(sigset, SIGUSR1);

    //block SIGUSR1 so it isn't handled immediately, allowing sigwait() to work
    if (sigprocmask(SIG_BLOCK, sigset, NULL) < 0) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }
}

pid_t fork_child_process(char *line, sigset_t *sigset) {
    char *args[MAX_ARGS];
    parse_command(line, args);

    pid_t pid = fork();
    if (pid < 0) {
        //fork failed
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        //child process waits for SIGUSR1 before executing the command
        printf("Child %d waiting for SIGUSR1...\n", getpid());
        int sig;
        sigwait(sigset, &sig); //wait until SIGUSR1 is received

        printf("Child %d received SIGUSR1, executing: %s\n", getpid(), args[0]);
        execvp(args[0], args);

        perror("execvp"); //if execvp fails (if it returns)
        exit(EXIT_FAILURE);
    }

    //parent returns child's PID
    return pid;
}

void signal_children(pid_t *pids, int count) {
    sleep(1); //allow time for children to block on sigwait()

    //send SIGUSR1 to all children to wake them up and let them exec
    printf("\nMCP: Sending SIGUSR1 to all children...\n");
    for (int i = 0; i < count; i++) {
        kill(pids[i], SIGUSR1);
    }

    sleep(1); //allow processes to begin executing

    //send SIGSTOP to suspend all children
    printf("\nMCP: Sending SIGSTOP to all children...\n");
    for (int i = 0; i < count; i++) {
        kill(pids[i], SIGSTOP);
    }

    sleep(1); //pause before resuming

    //send SIGCONT to resume all children
    printf("\nMCP: Sending SIGCONT to all children...\n");
    for (int i = 0; i < count; i++) {
        kill(pids[i], SIGCONT);
    }
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
