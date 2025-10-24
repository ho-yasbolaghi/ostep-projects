#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_PATHS 100
#define MAX_TOKENS 100
#define MAX_COMMANDS 10
#define MAX_ARGS 100

void free_array(char *array[], int size);
int tokenize_line(char *line, char *tokens[]);
char *build_executable_path(char *path[], int num_paths, char *exe);
void execute_command(char *exepath, char *tokens[], int num_tokens) __attribute__((noreturn));
pid_t command_handler(char *path[], int num_paths, char *tokens[], int num_tokens);
pid_t execute_loop_command(char *path[], int num_paths, char *tokens[], int num_tokens);

char error_message[30] = "An error has occurred\n";

int main(int argc, char *argv[]){
    char *path[MAX_PATHS]; // TODO: make dynamic to remove size limits
    int num_paths = 0;
    path[num_paths++] = strdup("/bin");

    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    int batch_mode = 0;

    FILE *stream = stdin;
    if (argc == 2)
    {
        batch_mode = 1;
        stream = fopen(argv[1], "r");
        if (stream == NULL) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
    } else if (argc > 2)
    {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }

    if (!batch_mode)
    {
        printf("wish> ");
    }

    while ((nread = getline(&line, &len, stream)) != -1)
    {
        char *commands[MAX_COMMANDS]; // TODO: make dynamic to remove size limits
        int num_commands = 0;

        char *cmd;
        while ((cmd = strsep(&line, "&")) != NULL)
        {
            commands[num_commands++] = cmd;
        }

        int num_children = 0;

        for (int i = 0; i < num_commands; i++)
        {
            char *tokens[MAX_TOKENS]; // TODO: make dynamic to remove size limits
            int num_tokens = tokenize_line(commands[i], tokens);

            if (num_tokens == 0)
            {
                continue;
            }

            if (strcmp(tokens[0], "exit") == 0)
            {
                if (num_tokens != 1)
                {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    continue;
                }

                free(line);
                free_array(path, num_paths);
                for (int i = 0; i < num_children; i++)
                {
                    (void) wait(NULL);
                }
                return 0;
            } else if (strcmp(tokens[0], "cd") == 0)
            {
                if (num_tokens != 2)
                {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    continue;
                }

                int rc = chdir(tokens[1]);
                if (rc == -1)
                {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                }
            } else if (strcmp(tokens[0], "path") == 0)
            {
                free_array(path, num_paths);
                num_paths = 0;
                if (num_tokens > 1)
                {
                    for (int i = 1; i < num_tokens; i++)
                    {
                        path[num_paths++] = strdup(tokens[i]);
                    }
                }
            } else if (strcmp(tokens[0], "loop") == 0)
            {
                pid_t rc = execute_loop_command(path, num_paths, tokens, num_tokens);
                if (rc > 0) {
                    num_children++;
                } else {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                }
            } else
            {
                pid_t rc = command_handler(path, num_paths, tokens, num_tokens);

                if (rc > 0) {
                    num_children++;
                } else if (rc < 0) {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                }
            }
        }

        for (int i = 0; i < num_children; i++)
        {
            (void) wait(NULL);
        }

        if (!batch_mode)
        {
            printf("wish> ");
        }
    }

    free(line);
    free_array(path, num_paths);
    return 0;
}

void free_array(char *array[], int size) {
    for (int i = 0; i < size; i++)
    {
        free(array[i]);
    }
}

int tokenize_line(char *line, char *tokens[]) {
    int size = 0;

    char *token;
    while ((token = strsep(&line, " \t\n")) != NULL)
    {
        // skip empty tokens
        if (*token == '\0')
        {
            continue;
        }

        char *redirect_pos;
        while ((redirect_pos = strchr(token, '>')) != NULL)
        {
            // add part before > if non-empty
            if (redirect_pos != token)
            {
                *redirect_pos = '\0';
                tokens[size++] = token;
            }

            // add > as a separate token
            tokens[size++] = ">";

            token = redirect_pos + 1;
        }

        // add any remaining part after last >
        if (*token != '\0')
        {
            tokens[size++] = token;
        }
    }

    return size;
}

char *build_executable_path(char *path[], int num_paths, char *exe) {
    char *exepath = NULL;
    for (int i = 0; i < num_paths; i++)
    {
        exepath = (char *)malloc(strlen(path[i]) + 1 + strlen(exe) + 1);
        strcpy(exepath, path[i]);
        strcat(exepath, "/");
        strcat(exepath, exe);
        if (access(exepath, X_OK) == 0)
        {
            break;
        }
        free(exepath);
        exepath = NULL;
    }

    return exepath;
}

void execute_command(char *exepath, char *tokens[], int num_tokens) {
    char *cmd_argv[MAX_ARGS]; // TODO: make dynamic to remove size limits
    cmd_argv[0] = strdup(tokens[0]);
    int i = 1;
    for (; i < num_tokens; i++)
    {
        if (strcmp(tokens[i], ">") == 0)
        {
            break;
        }

        cmd_argv[i] = strdup(tokens[i]);
    }

    cmd_argv[i] = NULL;

    if (i != num_tokens)
    {
        if (num_tokens - i != 2)
        {
            // after redirection symbol only one more argument is allowed
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }

        int fd = open(tokens[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd < 0)
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }

        (void) dup2(fd, STDOUT_FILENO);
        (void) dup2(fd, STDERR_FILENO);
        (void) close(fd);
    }

    execv(exepath, cmd_argv);
    free_array(cmd_argv, i); // technically unnecessary before exit
    exit(1);
}

pid_t command_handler(char *path[], int num_paths, char *tokens[], int num_tokens) {
    char *exepath = build_executable_path(path, num_paths, tokens[0]);

    if (exepath == NULL)
    {
        return -1;
    }

    pid_t rc = fork();

    if (rc == 0)
    {
        execute_command(exepath, tokens, num_tokens);
    } else if (rc > 0)
    {
        free(exepath);
        return rc;
    }

    free(exepath);
    return -1;
}

pid_t execute_loop_command(char *path[], int num_paths, char *tokens[], int num_tokens) {
    if (num_tokens < 3) {
        return -1;
    }

    int loop_count = atoi(tokens[1]);
    if (loop_count <= 0) {
        return -1;
    }

    pid_t rc = fork();

    if (rc > 0) {
        return rc;
    } else if (rc < 0) {
        return -1;
    }

    for (int loop_var = 1; loop_var <= loop_count; loop_var++) {
        char loop_str[11];
        sprintf(loop_str, "%d", loop_var);

        char *loop_tokens[MAX_TOKENS];
        int loop_num_tokens = 0;

        for (int i = 2; i < num_tokens; i++) {
            if (strcmp(tokens[i], "$loop") == 0) {
                loop_tokens[loop_num_tokens++] = loop_str;
            } else {
                loop_tokens[loop_num_tokens++] = tokens[i];
            }
        }

        rc = command_handler(path, num_paths, loop_tokens, loop_num_tokens);

        if (rc > 0) {
            (void) wait(NULL);
        } else if (rc < 0) {
            write(STDERR_FILENO, error_message, strlen(error_message));
        }
    }

    exit(0);
}
