#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>

#define MAX_CMDS 100
#define MAX_LINE 1024
#define MAX_ARGS 100
#define TIME_SLICE 1 // seconds

//created for part1
void trim_newline(char *str);
int read_input_file(const char *filename, char lines[][MAX_LINE]);
void parse_command(char *line, char **args);

//created for part2
void setup_sigusr1_blocking(sigset_t *sigset); //sets up signal blocking so child processes can wait for SIGUSR1
pid_t fork_child_process(char *line, sigset_t *sigset); //forks a new child process and has it wait for SIGUSR1 before calling execvp

//created for part4
void print_proc_stats(pid_t pid);
void handle_alarm(int sig);

typedef struct {
    pid_t pid;
    char cmd[MAX_LINE];
    int finished;
} process_t;

process_t processes[MAX_CMDS];
int proc_count = 0;
int current = -1;

int main(int argc, char *argv[]) {
    char lines[MAX_CMDS][MAX_LINE];
    sigset_t sigset;
    struct sigaction sa;

    setup_sigusr1_blocking(&sigset);
    int line_count = read_input_file("input.txt", lines);

    for (int i = 0; i < line_count; ++i) {
        if (strlen(lines[i]) == 0) continue;
        fork_child_process(lines[i], &sigset);
    }

    for (int i = 0; i < proc_count; i++) {
        kill(processes[i].pid, SIGUSR1);
    }

    sleep(1);
    for (int i = 0; i < proc_count; i++) {
        kill(processes[i].pid, SIGSTOP);
    }

    sa.sa_handler = handle_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    alarm(TIME_SLICE);
    while (1) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid > 0) {
            for (int i = 0; i < proc_count; i++) {
                if (processes[i].pid == pid) {
                    processes[i].finished = 1;
                    printf("MCP: Process %d (%s) finished\n", pid, processes[i].cmd);
                    break;
                }
            }

            int all_done = 1;
            for (int i = 0; i < proc_count; i++) {
                if (!processes[i].finished) {
                    all_done = 0;
                    break;
                }
            }
            if (all_done) break;
        }
        pause(); // wait for SIGALRM
    }

    printf("MCP: All processes have completed.\n");
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

void print_proc_stats(pid_t pid) {
    //prints basic stats from /proc/[pid]/stat and /status
    char path[64], buffer[1024];
    FILE *fp;

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    fp = fopen(path, "r");
    if (fp) {
        unsigned long utime, stime, rss;
        int ignore;
        char comm[256];
        fscanf(fp, "%d %s", &ignore, comm);
        for (int i = 0; i < 11; ++i) fscanf(fp, "%*s");
        fscanf(fp, "%lu %lu", &utime, &stime);
        fclose(fp);
        printf("PID %d (%s): utime=%lu, stime=%lu ", pid, comm, utime, stime);
    }

    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    fp = fopen(path, "r");
    if (fp) {
        while (fgets(buffer, sizeof(buffer), fp)) {
            if (strncmp(buffer, "VmRSS:", 6) == 0) {
                printf("%s", buffer);
                break;
            }
        }
        fclose(fp);
    }
}

void handle_alarm(int sig) {
    //signal handler for SIGALRM to simulate time-slice switching
    if (current >= 0 && !processes[current].finished) {
        kill(processes[current].pid, SIGSTOP);
    }

    int next = (current + 1) % proc_count;
    int loop_count = 0;

    while (processes[next].finished && loop_count < proc_count) {
        next = (next + 1) % proc_count;
        loop_count++;
    }

    if (!processes[next].finished) {
        current = next;
        kill(processes[current].pid, SIGCONT);
        printf("MCP: Running PID %d - %s\n", processes[current].pid, processes[current].cmd);
        print_proc_stats(processes[current].pid);
        alarm(TIME_SLICE);
    }
}

