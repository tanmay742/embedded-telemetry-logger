#include <iostream>
#include "logger_client.h"

#ifdef _WIN32
    #include <windows.h>
    #define sleep_ms(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define sleep_ms(ms) usleep((ms) * 1000)
#endif

int main() {
    std::cout << "[*] Target App started via Qt Toolchain.\n";

    // 1. Initialize connection to the background daemon on localhost.
        // NOTE: If the daemon runs on a separate development machine,
        // replace "127.0.0.1" with that computer's actual LAN IP address.
    if (!connect_logger("127.0.0.1", 9999)) {
        std::cerr << "[ERROR] Failed to initialize socket connection.\n";
        return 1;
    }
    std::cout << "[*] Transmitting data to background logger daemon...\n";

    int iteration = 0;
    float simulated_temp = 36.0f;

    while (true) {
        iteration++;
        simulated_temp += (iteration % 3 == 0) ? 0.5f : -0.1f;

        // Fire logs across varying classifications
        send_to_logger("[INFO] Target Runtime Tick: %d", iteration);
        send_to_logger("[INFO] Processing Unit Telemetry - Temp: %.2f C", simulated_temp);

        if (simulated_temp > 37.5f) {
            send_to_logger("[WARN] Core temperature elevation detected: %.2f C", simulated_temp);
        }

        if (iteration % 5 == 0) {
            send_to_logger("[ERROR] DMA Channel validation failure at block offset: 0x%X", iteration * 16);
        }

        std::cout << " -> Dispatched log sequence step #" << iteration << "\n";
        sleep_ms(1000); // Wait 1 second
    }

    disconnect_logger();
    return 0;
}
