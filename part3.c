#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_ARGS 100
#define MAX_CMDS 100

//global variables
char lines[MAX_CMDS][MAX_LINE];
pid_t pids[MAX_CMDS];
int is_finished[MAX_CMDS] = {0};
int count = 0;
int current = 0;

//created for part1
void trim_newline(char *str);
int read_input_file(const char *filename, char lines[][MAX_LINE]);
void parse_command(char *line, char **args);

//created for part2
void setup_sigusr1_blocking(sigset_t *sigset); //sets up signal blocking so child processes can wait for SIGUSR1
pid_t fork_child_process(char *line, sigset_t *sigset); //forks a new child process and has it wait for SIGUSR1 before calling execvp
void signal_children(pid_t *pids, int count); //sends SIGUSR1, SIGSTOP, and SIGCONT to all child processes

//created for part3
void alarm_handler(int signum);

int main(int argc, char *argv[]) {

    sigset_t sigset;
    setup_sigusr1_blocking(&sigset);  //setup signal blocking so child processes can use sigwait() to pause before exec
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    const char *filename = argv[1];
    int line_count = read_input_file(filename, lines); //store inputs in lines and get count

    for (int i = 0; i < count; i++) {
        if (strlen(lines[i]) == 0) continue;
        pids[i] = fork_child_process(lines[i], &sigset); //fork a child process for each command and store their PIDs
    }

    signal_children(pids, count); //send SIGUSR1 to children to unblock them so they exec(), then SIGSTOP to pause them

    //SIGALRM handler
    struct sigaction sa;
    sa.sa_handler = alarm_handler; //function to call on SIGALRM
    sa.sa_flags = SA_RESTART; //restart interrupted system calls
    sigemptyset(&sa.sa_mask); //don't block any signals during handler
    sigaction(SIGALRM, &sa, NULL); //register handler

    alarm(1); //start scheduler

    //monitor child processes
    while (1) {
        int all_done = 1;
        for (int i = 0; i < count; i++) { //loop through all children to check if they've exited
            if (!is_finished[i]) {
                int status;
                pid_t result = waitpid(pids[i], &status, WNOHANG);
                if (result == 0) {
                    all_done = 0; //child process still running
                } else if (result == pids[i]) {
                    is_finished[i] = 1; //child process has exited
                    printf("MCP: Process %d has exited.\n", pids[i]);
                }
            }
        }

        if (all_done) break; //break loop once all children have finished
        pause(); //wait for next SIGALRM
    }

    printf("MCP: All processes have finished.\n");
    return 0;

}

void alarm_handler(int signum) {
    if (count == 0) return;

    //stop the current process if it's still running
    if (!is_finished[current]) {
        kill(pids[current], SIGSTOP);
        printf("MCP: Stopped process %d\n", pids[current]);
    }

    //find the next active (unfinished) process
    int next = (current + 1) % count;
    while (is_finished[next]) {
        next = (next + 1) % count;
        if (next == current) {
            //all processes are finished or nothing left to schedule
            alarm(0); //cancel further alarms
            return;
        }
    }

    current = next;
    kill(pids[current], SIGCONT);
    printf("MCP: Continued process %d\n", pids[current]);

    alarm(1); //schedule the next alarm
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
        fprintf(stderr, "Child %d failed to exec command: %s\n", getpid(), args[0]);
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

