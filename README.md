# UNIX Memory Usage Monitor

A UNIX-based C project that simulates memory usage monitoring across multiple worker processes. Each worker reads binary files of configurable size and sends signals to the parent process upon exceeding memory limits or task completion.

## 📌 Features

- Generates large binary files filled with dummy data
- Forks multiple child worker processes
- Dynamically monitors memory usage per worker via `/proc/self/status`
- Sends signals (`SIGUSR1`, `SIGUSR2`) to notify the parent of:
  - Memory threshold exceedance
  - Task completion
- Logs system events to a `syslog.log` file with timestamps
- Uses `fcntl` locking to prevent log write conflicts between processes

## 🛠️ Technologies Used

- C (Standard Library)
- POSIX System Calls
- UNIX Signals
- File Descriptors & Locking
- `/proc` Filesystem (Linux-specific)

## 📁 File Structure

```
.
├── main.c          # Main source file containing all logic
├── syslog.log      # Auto-generated log file with memory and process events
├── workerX.bin     # Binary files generated during runtime
```

## 🧪 How It Works

1. The user inputs file sizes (in MB) for three workers.
2. `main.c` generates three binary files with the given sizes.
3. Three worker processes are forked.
4. Each worker:
   - Reads its respective binary file.
   - Allocates 50MB memory to ensure memory usage goes high.
   - Sends `SIGUSR1` if it exceeds 50MB usage.
   - Sends `SIGUSR2` after finishing.
5. The parent listens for signals and logs them into `syslog.log`.

## 🧾 Example Logs

```
[2025-04-18 15:42:31] ⚠️ Worker (PID: 4567) exceeded memory limit!
[2025-04-18 15:42:34] ✅ Worker (PID: 4567) completed.
```

## ▶️ Getting Started

### Compile
```bash
gcc main.c -o mem_monitor
```

### Run
```bash
./mem_monitor
```

Then input sizes like:
```
Enter file size for Worker 1 (MB): 5
Enter file size for Worker 2 (MB): 50
Enter file size for Worker 3 (MB): 500
```

## 📋 Notes

- Tested on Linux systems only (due to `/proc/self/status` usage)
- Log file is auto-created as `syslog.log`
- Signals used: `SIGUSR1` for memory alerts, `SIGUSR2` for completion
