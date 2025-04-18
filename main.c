#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

// Define the size of 1MB for memory allocation purposes
#define MB (1024 * 1024)

/**
 * @brief Generates a binary file filled with dummy data ('X') of specified size.
 *
 * @param filename The name of the file to create.
 * @param size_mb The size of the file in megabytes.
 */
void bn_generate_binary_file(const char *filename, int size_mb) {
    // Open the file for writing, create it if it doesn't exist, and truncate it if it does
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("File creation failed");
        exit(1);  // Exit if file creation fails
    }

    // Buffer to fill the file with dummy data
    char buffer[MB];
    memset(buffer, 'X', sizeof(buffer)); // Fill with 'X' characters
    
    // Write the buffer to the file the specified number of times (based on the file size)
    for (int i = 0; i < size_mb; i++) {
        write(fd, buffer, sizeof(buffer));
    }

    // Close the file descriptor
    close(fd);
}

/**
 * @brief Logs events (e.g., memory exceedance or process completion) into a log file.
 *
 * @param message The message to be logged.
 */
void bn_log_event(const char *message) {
    // Open log file for appending; create it if it doesn't exist
    int fd = open("syslog.log", O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd < 0) {
        perror("Log file open failed");
        return;  // Return if log file cannot be opened
    }

    // Initialize file lock structure to prevent other processes from modifying the log at the same time
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_pid = getpid();

    // Attempt to acquire the lock before writing to the log file
    if (fcntl(fd, F_SETLK, &lock) == -1) {
        perror("File lock failed");
        close(fd);
        return;  // Return if file lock fails
    }

    // Get the current timestamp
    time_t now;
    time(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // Format the log entry and write it to the log file
    char log_entry[256];
    snprintf(log_entry, sizeof(log_entry), "[%s] %s\n", timestamp, message);
    write(fd, log_entry, strlen(log_entry));

    // Release the lock
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);

    // Close the log file
    close(fd);
}

/**
 * @brief Worker process function that processes a binary file and sends signals based on memory usage.
 *
 * @param filename The name of the binary file to process.
 * @param parent_pid The PID of the parent process.
 */
 void bn_worker_process(const char *filename, pid_t parent_pid) {
    // Open the file for reading
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Worker: Failed to open file");
        exit(1);  // Exit if file cannot be opened
    }

    // Allocate 50MB of memory to read from the file
    char *buffer = malloc(50 * MB);  // Allocate 50MB to ensure memory threshold is hit
    if (!buffer) {
        perror("Failed to allocate memory");
        exit(1);  // Exit if memory allocation fails
    }

    ssize_t bytes_read;
    size_t total_read = 0;
    int mem_usage = 0;
    int signal_sent = 0;  // To prevent multiple signals for the same worker

    // Read from the file in chunks until EOF
    while ((bytes_read = read(fd, buffer + total_read, 4096)) > 0) {
        total_read += bytes_read;

        // Check memory usage from /proc/self/status
        FILE *status_file = fopen("/proc/self/status", "r");
        if (!status_file) {
            perror("Failed to open /proc/self/status");
            exit(1);  // Exit if status file cannot be opened
        }

        char line[256];
        while (fgets(line, sizeof(line), status_file)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                sscanf(line + 6, "%d", &mem_usage);  // Extract memory usage in kilobytes
                break;
            }
        }
        fclose(status_file);

        // If memory usage exceeds 50MB, send a signal to the parent process
        if (mem_usage > 50000 && !signal_sent) {
            kill(parent_pid, SIGUSR1);  // Notify parent that memory limit has been exceeded
            signal_sent = 1;  // Prevent further signals for this worker
        }
    }


    // Clean up
    free(buffer);
    close(fd);

    // Notify the parent that the worker has completed its task
    kill(parent_pid, SIGUSR2);
    exit(0);  // Exit the worker process
}


/**
 * @brief Signal handler to handle memory exceedance (SIGUSR1) and process completion (SIGUSR2).
 *
 * @param sig The signal number received.
 * @param info Information about the signal.
 * @param context The context of the signal (unused here).
 */
void bn_signal_handler(int sig, siginfo_t *info, void *context) {
    pid_t sender_pid = info->si_pid;
    if (sig == SIGUSR1) {
        // Log and print a warning when a worker exceeds memory usage
        char log_message[100];
        snprintf(log_message, sizeof(log_message), "⚠️ Worker (PID: %d) exceeded memory limit!", sender_pid);
        bn_log_event(log_message);
        printf("%s\n", log_message);
    } else if (sig == SIGUSR2) {
        // Log and print a message when a worker completes its task
        char log_message[100];
        snprintf(log_message, sizeof(log_message), "✅ Worker (PID: %d) completed.", sender_pid);
        bn_log_event(log_message);
        printf("%s\n", log_message);
    }
}

/**
 * @brief Sets up signal handlers for SIGUSR1 and SIGUSR2.
 */
void bn_setup_signals() {
    struct sigaction sa;
    sa.sa_sigaction = bn_signal_handler;
    sa.sa_flags = SA_SIGINFO;  // Specify that the handler will receive extra info
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);  // Set handler for memory exceedance signal
    sigaction(SIGUSR2, &sa, NULL);  // Set handler for process completion signal
}

int main() {
    int size1, size2, size3;

    // Get file sizes for three worker processes
    printf("Enter file size for Worker 1 (MB): ");
    scanf("%d", &size1);
    printf("Enter file size for Worker 2 (MB): ");
    scanf("%d", &size2);
    printf("Enter file size for Worker 3 (MB): ");
    scanf("%d", &size3);

    // Generate binary files for the workers
    bn_generate_binary_file("worker1.bin", size1);
    bn_generate_binary_file("worker2.bin", size2);
    bn_generate_binary_file("worker3.bin", size3);

    printf("Binary files created.\n");

    // Set up signal handlers
    bn_setup_signals();

    // Get the PID of the parent process
    pid_t parent_pid = getpid();

    // Create worker processes
    pid_t pid1 = fork();
    if (pid1 == 0) {
        bn_worker_process("worker1.bin", parent_pid);  // Worker 1
    } else {
        pid_t pid2 = fork();
        if (pid2 == 0) {
            bn_worker_process("worker2.bin", parent_pid);  // Worker 2
        } else {
            pid_t pid3 = fork();
            if (pid3 == 0) {
                bn_worker_process("worker3.bin", parent_pid);  // Worker 3
            } else {
                // Parent process waits for all worker processes to complete
                waitpid(pid1, NULL, 0);
                waitpid(pid2, NULL, 0);
                waitpid(pid3, NULL, 0);
            }
        }
    }
    return 0;
}
