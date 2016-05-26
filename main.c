#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wordexp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

#define COMMAND_MAX_LENGTH 10
#define MAX_ARGS_LENGTH 1024
#define MAX_PATH_LENGTH 1024

typedef enum {
    HELP, QUIT, APP, UNDEFINED
} Command;

int read_word(char * buffer, int n);
void parse_command(const char * command_str, Command *command, int *chosen_app_id);
void display_apps();
int read_app_args(int app_id, char *main_path, wordexp_t *parsed_args);
char *get_app_path(int app_id, char *main_path);

void sigchld_handler(int signum) {}
void sigint_handler(int signum);

int apps_num = 1;
char *apps_names[] = {
        "Aircraft carrier"
};

char *apps_args[] = {
        "N, K and a number of planes"
};

char *apps_paths[] = {
        "apps/aircraft_carrier/aircraft_main"
};
int ignore_close = 0;

int main(int argc, char *argv[]) {
    struct sigaction sigchld_action, sigint_action;
    sigchld_action.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sigchld_action, NULL);
    sigint_action.sa_handler = sigint_handler;
    sigaction(SIGINT, &sigint_action, NULL);
    sigaction(SIGTSTP, &sigint_action, NULL);

    char command_buffer[COMMAND_MAX_LENGTH];
    Command command = UNDEFINED;
    int chosen_app_id;
    wordexp_t parsed_args;
    sigset_t mask, wait_mask, old_mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGTSTP);
    sigdelset(&mask, SIGINT);
    sigfillset(&wait_mask);
    sigdelset(&wait_mask, SIGCHLD);
    sigprocmask(SIG_SETMASK, &mask, &old_mask);
    do {
        printf("$ ");
        if (read_word(command_buffer, COMMAND_MAX_LENGTH) == 0)
            continue;
        parse_command(command_buffer, &command, &chosen_app_id);
        switch (command) {
            case HELP:
                printf("Choose one of apps below. To exit, enter quit or q.\n");
                display_apps();
                break;
            case QUIT:
                break;
            case UNDEFINED:
                printf("Undefined command. Use command help, to read about possible commands.\n");
                break;
            case APP:
                printf("Chosen %d\n", chosen_app_id + 1);
                char *app_path = get_app_path(chosen_app_id, argv[0]);
                if (read_app_args(chosen_app_id, app_path, &parsed_args) != 0) {
                    printf("Incorrect arguments.\n");
                    break;
                }
                printf ("Args ok.\n");
                fflush(stdout);
                pid_t pid = fork();
                if (pid < 0) {
                    printf("Error while creating new process occurred.\n");
                }
                else if (pid == 0) {
                    sigprocmask(SIG_SETMASK, &old_mask, NULL);
                    execv(app_path, parsed_args.we_wordv);
                }
                else {
                    ignore_close = 1;
                    sigsuspend(&wait_mask);
                    waitpid(pid, NULL, 0);
                }
                wordfree(&parsed_args);
                free(app_path);
                break;
        }
    } while (command != QUIT);
    printf("\nApplication closed.\n");
    return 0;
}

void display_apps() {
    for (int i = 0; i < apps_num; i++) {
        printf("%d %s\n", i + 1, apps_names[i]);
    }
}

int read_app_args(int app_id, char *app_path, wordexp_t *parsed_args) {
    char args[MAX_ARGS_LENGTH];
    strcpy(args, app_path);
    if (strlen(apps_args[app_id]) > 0) {
        char c;
        int i;
        printf("Enter following args - %s: ", apps_args[app_id]);
        i = (int)strlen(args);
        args[i] = ' ';
        i++;
        c = (char)getchar();
        while (c != '\n' && i < MAX_ARGS_LENGTH - 1) {
            args[i] = c;
            i++;
            c = (char)getchar();
        }
        while (c != '\n') c = (char)getchar();
        args[i] = '\0';
    }
    return wordexp(args, parsed_args, 0) != 0;
}

int read_word(char * buffer, int n) {
    buffer[0] = '\0';
    char c;
    int i = 0;
    do
        c = (char)getchar();
    while (c == ' ' || c == '\t');
    while (c != '\n' && c != ' ' && c != '\t' && i < n - 1) {
        *buffer = c;
        buffer++;
        i++;
        c = (char)getchar();
    }
    if (i == n - 1)
        while (c != '\n' && c != ' ' && c != '\t')
            c = (char)getchar();
    *buffer = '\0';
    return i;
}

void parse_command(const char * command_str, Command *command, int *chosen_app_id) {
    int app_id = atoi(command_str);
    if (strcmp(command_str, "help") == 0)
        *command = HELP;
    else if (strcmp(command_str, "quit") == 0 || strcmp(command_str, "q") == 0)
        *command = QUIT;
    else if (app_id > 0 && app_id <= apps_num) {
        *command = APP;
        *chosen_app_id = app_id - 1;
    }
    else {
        *command = UNDEFINED;
    }
}

char *get_app_path(int app_id, char *main_path) {
    char *path = malloc(MAX_PATH_LENGTH);
    if (path == NULL) {
        printf("Error while allocating memory occurred.\n");
        exit(0);
    }
    size_t end_of_path_index = strlen(main_path) - 1;
    while (main_path[end_of_path_index] != '/')
        end_of_path_index--;
    strncpy(path, main_path, end_of_path_index + 1);
    path[end_of_path_index + 1] = '\0';
    strcat(path, apps_paths[app_id]);
    return path;
}

void sigint_handler(int signum) {
    if (ignore_close == 1)
        ignore_close = 0;
    else
        exit(0);
}

