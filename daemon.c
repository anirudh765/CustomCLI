#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include "auto_delete.h"

#define DAEMON_NAME       "auto_delete_daemon"
#define CHECK_INTERVAL    60  

volatile sig_atomic_t running = 1;

void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        syslog(LOG_INFO, "Received signal %d, preparing to shut down", sig);
        running = 0;
    }
}

void daemonize() {
    pid_t pid, sid;

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);  
    umask(0);
    openlog(DAEMON_NAME, LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Starting daemon");

    sid = setsid();
    if (sid < 0) {
        syslog(LOG_ERR, "Failed to create new SID");
        exit(EXIT_FAILURE);
    }

    if (chdir("/") < 0) {
        syslog(LOG_ERR, "Failed to change working directory to /");
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main() {
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    signal(SIGHUP, handle_signal);  
    
    daemonize();

    AutoDeleteSystem system;
    if (!init_system(&system)) {
        syslog(LOG_ERR, "Could not initialize auto-delete system");
        closelog();
        exit(EXIT_FAILURE);
    }

    syslog(LOG_INFO, "Auto-delete daemon initialized successfully");
    syslog(LOG_INFO, "Running purge check every %d seconds", CHECK_INTERVAL);
    syslog(LOG_INFO, "Recycle bin path: %s", system.recycle_bin);
    syslog(LOG_INFO, "Database path: %s", system.db_path);

    // Main daemon loop
    while (running) {
        syslog(LOG_INFO, "Starting daemon check cycle");
        
        syslog(LOG_INFO, "Checking for expired files");
        char* result = purge_expired(&system);
        if (result) {
            syslog(LOG_INFO, "Purge result: %s", result);
            free(result);
        } else {
            syslog(LOG_ERR, "purge_expired returned NULL");
        }
        
        if (!running) {
            break;
        }
        
        syslog(LOG_INFO, "Sleeping for %d seconds before next check", CHECK_INTERVAL);
        
        for (int i = 0; i < CHECK_INTERVAL && running; i++) {
            sleep(1);
        }
    }

    syslog(LOG_INFO, "Daemon shutting down");
    cleanup_system(&system);
    closelog();
    return EXIT_SUCCESS;
}