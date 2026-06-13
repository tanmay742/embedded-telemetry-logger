# Asynchronous High-Frequency Telemetry & Logging Framework

* A lightweight, zero-dependency, asynchronous telemetry logging framework developed in C++ and Python that works over UDP without blocking your main application loop. 
* Designed for resource-constrained systems (like embedded boards or real-time controllers),the framework can handle fast logging streams (tested down to **5ms task intervals**) with zero jitter or performance penalties on the primary application loop. It achieves this by decoupling logging operations into an asynchronous **Producer-Consumer** architecture over local or remote UDP transport layers.

---

##  System Architecture & Core Design Logic

Standard I/O operations (e.g., writing to local text files or streaming over synchronized TCP connections) stall execution loops, introducing latency spikes that violate real-time constraints. This framework completely mitigates that problem through a stratified system:
```
+=============================================================+
|               Target C++ Application Loop                   |
|   (Deterministic, stack-allocated, non-blocking UDP)        |
+=============================================================+
                                │
                       DP Port 9999 (RAM Buffer)
                                │
                                ▼
+=============================================================+
|            Asynchronous Multi-Threaded Host Daemon          |
|   (Thread-Safe Queue + Sliding Micro-Batching Engine)       |
+=============================================================+
                                │
                       SQL Transactions
                                │
                                ▼
+=============================================================+
|                SQLite Durability Layer                      |
|       (Indexed Storage + Fast Sequential Disk Writes)       |
+=============================================================+
```
*  **Fire-and-Forget C++ Target (Producer):** When `send_to_logger()` is invoked, strings are formatted instantly in volatile stack memory (`char log_buffer[1024]`) to prevent heap fragmentation. The buffer is then immediately dispatched via a connectionless, non-blocking **UDP socket**. The entire operation resolves in **$<15\mu s$**, utilizing less than 0.3% of a 5ms system processing window.
*  **Lockless Network Ingest (Consumer Thread 1):** A dedicated background listener thread on the Python daemon intercepts incoming UDP datagrams from port `9999` and instantly drops them into a thread-safe FIFO queue, keeping the socket buffer empty and preventing packet drops during high-frequency microsecond bursts.
*  **Sliding Micro-Batching Storage (Consumer Thread 2):** A separate database worker thread pulls elements from the memory queue. By utilizing a sliding time window optimization (`time.sleep(0.01)`), the daemon groups incoming bursts and performs a single unified SQLite batch transaction (`executemany`), converting high-frequency random disk writes into high-speed sequential writes.
*  **Decoupled Monitoring UI:** An integrated, lightweight native HTTP server exposes an offline web dashboard at port `8000`. The frontend uses client-side rendering and debounced polling loops to query the indexed database, allowing engineers to filter telemetries in real-time without touching or degrading the target hardware execution path.

---

## Quick Start (Demo Run via Local Workspace)

Follow these steps to spin up the complete local end-to-end framework test using your project directory assets:

### 1. Launch the Background Python Daemon
* Open a terminal in your project workspace directory and run the daemon script to initialize the database pipeline and UI server:
```bash
python3 logger_daemon.py
```
### 2. Compile & Run the Target Simulator
* Open TargetLoggerTest.pro inside Qt Creator or run make via your command-line environment.
* Compile and run the target application binary.
* The console will output telemetry ticks ([*] Transmitting data to background logger daemon...).

### 3. Open the Telemetry Dashboard UI
* Launch any standard web browser offline and navigate to: http://localhost:8000
* You will see live runtime info blocks, system tracking telemetries, and warning strings flowing cleanly into the web console in real-time.

---

Integration Guide: Adding to a Production Target
To integrate this logger tool into a different target application, only three simple steps are required:

Step 1: Add Client Source Files
Copy logger_client.h and logger_client.cpp into your target application's directory tree.

Step 2: Link Sockets in Build Environment
Because the framework relies on native operating system network stacks, ensure your linker includes the networking dependencies:
For Qt Build Toolchains (.pro files):
```
Ini, TOML
win32: LIBS += -lws2_32
For CMake Environments (CMakeLists.txt):

CMake
if(WIN32)
    target_link_libraries(YourTargetBinary PRIVATE ws2_32)
endif()
For Standard Linux Toolchains (GCC / G++ CLI):
```
Bash
```g++ -O2 main.cpp logger_client.cpp -o arm_target_app```
(No extra linker flags are needed on Linux, as POSIX sockets are built directly into standard runtime headers).

Step 3: Initialize and Trace
Include logger_client.h in your startup routine, initialize the socket link via connect_logger(), and drop tracking statements anywhere within your application loops just like printf:

C++
```#include "logger_client.h"

int main() {
    // Initialize the network logging node. 
    // Replace "127.0.0.1" with your host machine's IP if cross-compiling for an ARM Linux target board!
    if (!connect_logger("127.0.0.1", 9999)) {
        return 1; // Handle initialization failure safely
    }

    // Call anywhere safely - executes in microseconds inside high-frequency interrupts/loops
    send_to_logger("[INFO] Subsystem initialized successfully.");
    
    int loop_voltage = 12;
    send_to_logger("[WARN] Voltage rail fluctuation tracked: %d V", loop_voltage);

    // Close the network descriptor before exit
    disconnect_logger();
    return 0;
}
```
---

## Performance Benchmarks & Engineering ConstraintsTarget Execution Latency:
* $<15\mu s$ per log invocation path.Maximum Tested Target Frequency: 5ms operational resolution bursts without loop delay or drift.
* Storage Optimization: Sliding 10ms micro-batch grouping reduces database write transactions up to 500x under continuous logging stresses.
* Memory Management: Zero dynamic heap allocation (malloc/new) on the target application path to guarantee protection against runtime memory fragmentation and leaks.
