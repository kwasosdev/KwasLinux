#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>
#include <signal.h>
#include <errno.h>

#define KWASH_VERSION "0.1"

// Прототипы встроенных команд
int kwash_cd(char **args);
int kwash_exit(char **args);
int kwash_clear(char **args);
int kwash_poweroff(char **args);
int kwash_reboot(char **args);

// Список встроенных команд и указатели на их функции
char *builtin_str[] = { 
    "cd", 
    "exit", 
    "clear", 
    "poweroff", 
    "shutdown", 
    "reboot" 
};

int (*builtin_func[]) (char **) = { 
    &kwash_cd, 
    &kwash_exit, 
    &kwash_clear, 
    &kwash_poweroff, 
    &kwash_poweroff, 
    &kwash_reboot 
};

int kwash_num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

int kwash_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "kwash: ожидается аргумент для \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) perror("kwash");
    }
    return 1;
}

int kwash_exit(char **args) {
    (void)args;
    return 0;
}

int kwash_clear(char **args) {
    (void)args;
    printf("\033[2J\033[H");
    return 1;
}

int kwash_poweroff(char **args) {
    (void)args;
    printf("Powering off...\n");
    kill(1, SIGTERM);
    exit(0);
}

int kwash_reboot(char **args) {
    (void)args;
    printf("Rebooting...\n");
    kill(1, SIGINT);
    exit(0);
}

void kwash_load_profile(void) {
    FILE *file = fopen("/etc/profile", "r");
    if (!file) {
        setenv("PATH", "/bin:/usr/bin", 1);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        char *equal_sign = strchr(line, '=');
        if (equal_sign) {
            *equal_sign = '\0';
            char *var_name = line;
            char *var_value = equal_sign + 1;
            
            var_value[strcspn(var_value, "\n")] = 0;
            if (var_value[0] == '"') var_value++; 
            int len = strlen(var_value);
            if (len > 0 && var_value[len-1] == '"') var_value[len-1] = '\0'; 

            setenv(var_name, var_value, 1);
        }
    }
    fclose(file);
}

void print_prompt(void) {
    char *ps1 = getenv("PS1");
    if (!ps1) {
        printf("kwash> ");
        return;
    }

    for (int i = 0; ps1[i] != '\0'; i++) {
        if (ps1[i] == '\\') {
            i++;
            switch (ps1[i]) {
                case 'u': {
                    struct passwd *pw = getpwuid(getuid());
                    printf("%s", pw ? pw->pw_name : "root");
                    break;
                }
                case 'h':
                    printf("kwasik");
                    break;
                case 'w': {
                    char cwd[1024];
                    if (getcwd(cwd, sizeof(cwd))) printf("%s", cwd);
                    break;
                }
                case 'e':
                    printf("\033");
                    break;
                case '[': case ']':
                    break;
                default:
                    printf("\\%c", ps1[i]);
                    break;
            }
        } else {
            putchar(ps1[i]);
        }
    }
    fflush(stdout);
}

int kwash_launch(char **args) {
    pid_t pid = fork();
    if (pid == 0) {
        if (execvp(args[0], args) == -1) perror("kwash");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("kwash");
    } else {
        int status;
        do { waitpid(pid, &status, WUNTRACED); } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 1;
}

int kwash_execute(char **args) {
    if (args[0] == NULL) return 1;

    if (strcmp(args[0], "ls") == 0) {
        int has_color = 0;
        int count = 0;
        while (args[count] != NULL) {
            if (count > 0 && strstr(args[count], "--color")) has_color = 1;
            count++;
        }
        
        if (!has_color) {
            for (int j = count; j >= 1; j--) {
                args[j + 1] = args[j];
            }
            args[1] = "--color=auto";
        }
    }

    for (int i = 0; i < kwash_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    return kwash_launch(args);
}

char *kwash_read_line(void) {
    char *line = NULL;
    size_t bufsize = 0;
    
    print_prompt();
    
    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) { printf("\n"); exit(EXIT_SUCCESS); }
        else { perror("kwash"); exit(EXIT_FAILURE); }
    }
    return line;
}

char **kwash_split_line(char *line) {
    int bufsize = 128;
    int position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) { fprintf(stderr, "kwash: malloc error\n"); exit(EXIT_FAILURE); }

    token = strtok(line, " \t\r\n\a");
    while (token != NULL) {
        tokens[position++] = token;
        if (position >= bufsize) {
            bufsize += 128;
            tokens = realloc(tokens, bufsize * sizeof(char*));
        }
        token = strtok(NULL, " \t\r\n\a");
    }
    tokens[position] = NULL;
    return tokens;
}

void kwash_loop(void) {
    char *line;
    char **args;
    int status;
    do {
        line = kwash_read_line();
        args = kwash_split_line(line);
        status = kwash_execute(args);
        free(line);
        free(args);
    } while (status);
}

int main(int argc, char **argv) {
    kwash_load_profile();

    if (argc > 1) {
        FILE *file = fopen(argv[1], "r");
        if (!file) {
            perror("kwash");
            return EXIT_FAILURE;
        }

        char *line = NULL;
        size_t len = 0;
        while (getline(&line, &len, file) != -1) {
            if (line[0] == '#' || line[0] == '\n') continue;
            char **args = kwash_split_line(line);
            kwash_execute(args);
            free(args);
        }
        free(line);
        fclose(file);
        return EXIT_SUCCESS;
    }

    printf("Kwas Shell (kwash) v%s\n", KWASH_VERSION);
    kwash_loop();
    
    return EXIT_SUCCESS;
}
