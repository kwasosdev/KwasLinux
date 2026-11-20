#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>

#define OS_NAME "Kwasik"
#define MAX_MODULES 512

volatile sig_atomic_t shutdown_requested = 0;
volatile sig_atomic_t reboot_requested = 0;
char *loaded_modules[MAX_MODULES];
int loaded_count = 0;

void load_module_with_deps(const char *mod_name);

void handle_sigchld(int sig) { (void)sig; while (waitpid(-1, NULL, WNOHANG) > 0); }
void handle_shutdown(int sig) {
    if (sig == SIGINT) reboot_requested = 1;
    else reboot_requested = 0;
    shutdown_requested = 1;
}
void log_msg(const char *msg) { printf("[" OS_NAME " init] %s\n", msg); fflush(stdout); }
void mkdir_if_missing(const char *path) { if (mkdir(path, 0755) != 0 && errno != EEXIST) { } }

int is_loaded(const char *mod_name) {
    for (int i = 0; i < loaded_count; i++) {
        if (strcmp(loaded_modules[i], mod_name) == 0) return 1;
    }
    return 0;
}

int wildcard_match(const char *pattern, const char *string) {
    if (*pattern == '\0') return *string == '\0';
    if (*pattern == '*') {
        while (*pattern == '*') pattern++;
        if (*pattern == '\0') return 1;
        while (*string) {
            if (wildcard_match(pattern, string)) return 1;
            string++;
        }
        return 0;
    }
    if (*pattern == *string) return wildcard_match(pattern + 1, string + 1);
    return 0;
}

void get_module_options(const char *mod_name, char *options, size_t max_len) {
    options[0] = '\0';
    
    DIR *dir = opendir("/etc/modprobe.d");
    if (!dir) return;

    char norm_mod[128];
    snprintf(norm_mod, sizeof(norm_mod), "%s", mod_name);
    for (int i = 0; norm_mod[i]; i++) if (norm_mod[i] == '_') norm_mod[i] = '-';

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".conf") == NULL) continue;

        char conf_path[512];
        snprintf(conf_path, sizeof(conf_path), "/etc/modprobe.d/%s", entry->d_name);
        
        FILE *conf = fopen(conf_path, "r");
        if (!conf) continue;

        char line[256];
        while (fgets(line, sizeof(line), conf)) {
            line[strcspn(line, "\r\n")] = 0;
            if (line[0] == '#' || line[0] == '\0') continue;

            char *ptr = strstr(line, "options ");
            if (!ptr) continue;
            
            ptr += 8;
            while (*ptr == ' ' || *ptr == '\t') ptr++;

            char mod_in_line[128] = {0};
            sscanf(ptr, "%127s", mod_in_line);

            char norm_mod_line[128];
            snprintf(norm_mod_line, sizeof(norm_mod_line), "%s", mod_in_line);
            for (int i = 0; norm_mod_line[i]; i++) if (norm_mod_line[i] == '_') norm_mod_line[i] = '-';
            if (strcmp(norm_mod_line, norm_mod) == 0) {
                char *args = ptr + strlen(mod_in_line);
                while (*args == ' ' || *args == '\t') args++;
                if (*args) {
                    size_t cur_len = strlen(options);
                    if (cur_len > 0 && cur_len < max_len - 1) {
                        snprintf(options + cur_len, max_len - cur_len, " %s", args);
                    } else {
                        snprintf(options, max_len, "%s", args);
                    }
                }
            }
        }
        fclose(conf);
    }
    closedir(dir);
}


void load_kernel_module_file(const char *full_path, const char *mod_name) {
    if (is_loaded(mod_name)) return;
    char path[512];
    struct utsname un;
    uname(&un);
    snprintf(path, sizeof(path), "/lib/modules/%s/%s", un.release, full_path);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("[" OS_NAME " init] Warning: Cannot open %s\n", path);
        return;
    }
    
    char options[512];
    get_module_options(mod_name, options, sizeof(options));
    if (options[0] != '\0') {
        printf("[" OS_NAME " init] Applying options for %s: %s\n", mod_name, options);
    }

    int ret = syscall(SYS_finit_module, fd, options, 0);
    close(fd);
    
    if (ret == 0 && loaded_count < MAX_MODULES) {
        loaded_modules[loaded_count++] = strdup(mod_name);
        printf("[" OS_NAME " init] Loaded module: %s\n", mod_name); 
    } else if (ret != 0) {
        printf("[" OS_NAME " init] Failed to load %s (errno: %d)\n", mod_name, errno); 
    }
}

void load_module_with_deps(const char *mod_name) {
    if (is_loaded(mod_name)) return;

    struct utsname un;
    uname(&un);
    char dep_path[512];
    snprintf(dep_path, sizeof(dep_path), "/lib/modules/%s/modules.dep", un.release);

    FILE *file = fopen(dep_path, "r");
    if (!file) return;

    char line[1024];
    
    char normalized_mod[512];
    char target_mod[sizeof(normalized_mod) + 4];
    
    snprintf(normalized_mod, sizeof(normalized_mod), "%s", mod_name);
    for (int i = 0; normalized_mod[i]; i++) {
        if (normalized_mod[i] == '_') normalized_mod[i] = '-';
    }
    
    snprintf(target_mod, sizeof(target_mod), "%s.ko", normalized_mod);

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            if (strstr(line, target_mod) != NULL) {
                char *deps = colon + 1;
                char *dep = deps;
                while (dep && *dep) {
                    while (*dep == ' ') dep++;
                    if (!*dep) break;
                    char *end = strchr(dep, ' ');
                    if (end) *end = '\0';

                    char dep_name[600];
                    char *last_slash = strrchr(dep, '/');
                    if (last_slash) snprintf(dep_name, sizeof(dep_name), "%s", last_slash + 1);
                    else snprintf(dep_name, sizeof(dep_name), "%s", dep);

                    char *dot = strstr(dep_name, ".ko");
                    if (dot) *dot = '\0';
                    
                    load_module_with_deps(dep_name);
                    dep = end ? end + 1 : NULL;
                }
                load_kernel_module_file(line, mod_name);
                break;
            }
        }
    }
    fclose(file);
}

void load_hardware_modules() {
    log_msg("Scanning hardware for required modules...");
    struct utsname un;
    uname(&un);
    char alias_path[512];
    snprintf(alias_path, sizeof(alias_path), "/lib/modules/%s/modules.alias", un.release);

    FILE *alias_file = fopen(alias_path, "r");
    if (!alias_file) {
        printf("[" OS_NAME " init] ERROR: modules.alias not found at %s!\n", alias_path);
        return;
    }

    DIR *dir = opendir("/sys/bus/pci/devices");
    if (!dir) {
        log_msg("Warning: /sys/bus/pci/devices not found!");
        fclose(alias_file);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char modalias_path[512];
        snprintf(modalias_path, sizeof(modalias_path), "/sys/bus/pci/devices/%s/modalias", entry->d_name);

        FILE *ma_file = fopen(modalias_path, "r");
        if (!ma_file) continue;

        char modalias[256];
        if (fgets(modalias, sizeof(modalias), ma_file)) {
            modalias[strcspn(modalias, "\n")] = 0;

            rewind(alias_file);
            char line[512];
            while (fgets(line, sizeof(line), alias_file)) {
                if (strncmp(line, "alias ", 6) == 0) {
                    char *pattern = line + 6;
                    char *module_name = strchr(pattern, ' ');
                    if (module_name) {
                        *module_name = '\0';
                        module_name++;
                        module_name[strcspn(module_name, "\n")] = 0;

                        if (wildcard_match(pattern, modalias)) {
                            printf("[" OS_NAME " init] Hardware match: %s -> %s\n", modalias, module_name);
                            load_module_with_deps(module_name);
                            break;
                        }
                    }
                }
            }
        }
        fclose(ma_file);
    }
    closedir(dir);
    fclose(alias_file);
    log_msg("Hardware scan complete.");
}

void kill_all_processes() {
    DIR *dir = opendir("/proc");
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (isdigit(entry->d_name[0])) {
            int pid = atoi(entry->d_name);
            if (pid > 1) kill(pid, SIGTERM);
        }
    }
    closedir(dir);
    sleep(1);
    dir = opendir("/proc");
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (isdigit(entry->d_name[0])) {
                int pid = atoi(entry->d_name);
                if (pid > 1) kill(pid, SIGKILL);
            }
        }
        closedir(dir);
    }
}

void run_script(const char *path) {
    if (access(path, F_OK) != 0) return;
    log_msg("Launching init script...");
    pid_t pid = fork();
    if (pid == 0) { execl("/bin/kwash", "kwash", path, NULL); _exit(127); }
    else if (pid > 0) { int status; waitpid(pid, &status, 0); }
}

pid_t spawn_shell(const char *tty_path) {
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
        int fd = open(tty_path, O_RDWR);
        if (fd >= 0) {
            ioctl(fd, TIOCSCTTY, 0);
            tcsetpgrp(fd, getpid()); // Привязываем терминал для Ctrl+C
            dup2(fd, 0); dup2(fd, 1); dup2(fd, 2);
            if (fd > 2) close(fd);
        } else { _exit(1); }
        execl("/bin/kwash", "kwash", NULL);
        _exit(127);
    }
    return pid;
}

int main(void) {
    if (getpid() != 1) return 1;
    signal(SIGCHLD, handle_sigchld);

    struct sigaction sa;
    sa.sa_handler = handle_shutdown;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    log_msg("Launching init system (kinits)");

    mkdir_if_missing("/proc"); mkdir_if_missing("/sys"); mkdir_if_missing("/dev");
    mkdir_if_missing("/dev/pts"); mkdir_if_missing("/run"); mkdir_if_missing("/tmp");

    mount(NULL, "/", NULL, MS_REMOUNT, NULL);
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mount("devpts", "/dev/pts", "devpts", 0, "mode=0620,gid=5");
    mount("tmpfs", "/run", "tmpfs", 0, "mode=0755");
    mount("tmpfs", "/tmp", "tmpfs", 0, "mode=1777");

    mknod("/dev/ttyS0", S_IFCHR | 0600, makedev(4, 64));
    mknod("/dev/tty1", S_IFCHR | 0600, makedev(4, 1));
    mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3));

    load_hardware_modules();

    run_script("/etc/kinits/boot.sh");
    run_script("/etc/kinits/user.sh");

    log_msg("Launching interactive environment...");
    pid_t shell_serial = spawn_shell("/dev/ttyS0");
    pid_t shell_graphical = spawn_shell("/dev/tty1");

    if (shell_serial > 0 || shell_graphical > 0) {
        int status;
        while (!shutdown_requested) {
            pid_t w = waitpid(-1, &status, 0);
            if (shutdown_requested) break;
            if (w == shell_serial) { sleep(1); shell_serial = spawn_shell("/dev/ttyS0"); }
            else if (w == shell_graphical) { sleep(1); shell_graphical = spawn_shell("/dev/tty1"); }
            else if (w < 0 && errno != EINTR) break;
        }
    }

    if (reboot_requested) log_msg("Rebooting Kwasik...");
    else log_msg("Shutting down Kwasik...");

    log_msg("Terminating all processes...");
    kill_all_processes();

    log_msg("Syncing and unmounting filesystems...");
    sync();
    umount("/tmp"); umount("/run"); umount("/dev/pts");
    umount("/dev"); umount("/sys"); umount("/proc");

    log_msg("Powering off in 2 seconds...");
    sleep(2);

    if (reboot_requested) reboot(RB_AUTOBOOT);
    else reboot(RB_POWER_OFF);
    return 0;
}
