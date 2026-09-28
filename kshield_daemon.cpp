#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <csignal>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <poll.h>
#include "kshield_common.h"

class SafeDescriptor {
public:
    explicit SafeDescriptor(int fd = -1) : fd_(fd) {}
    ~SafeDescriptor() { release(); }

    SafeDescriptor(const SafeDescriptor&) = delete;
    SafeDescriptor& operator=(const SafeDescriptor&) = delete;

    SafeDescriptor(SafeDescriptor&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    SafeDescriptor& operator=(SafeDescriptor&& other) noexcept {
        if (this != &other) {
            release();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int get() const { return fd_; }
    bool valid() const { return fd_ >= 0; }

    void release() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

private:
    int fd_;
};

static std::atomic<bool> g_shutdown{false};

void handle_signal(int) {
    g_shutdown = true;
}

class ThreatClassifier {
public:
    static bool isThreat(const kshield_telemetry& ev, std::string& reason) {
        std::string path(ev.target_path);

        // Rule 1: Execution from staging or memory-backed transient dirs
        if (path.rfind("/tmp/", 0) == 0 || 
            path.rfind("/dev/shm/", 0) == 0 || 
            path.rfind("/var/tmp/", 0) == 0) {
            reason = "Binary execution from prohibited staging/shared directory: " + path;
            return true;
        }

        // Rule 2: Unprivileged execution of network recon tools
        if (ev.uid != 0) {
            static const std::vector<std::string> lolbins = {
                "nmap", "nc", "netcat", "tcpdump", "wireshark", "masscan"
            };
            for (const auto& bin : lolbins) {
                if (std::string(ev.comm) == bin || path.find("/" + bin) != std::string::npos) {
                    reason = "Unprivileged user executed restricted network recon tool (" + bin + ")";
                    return true;
                }
            }
        }

        // Rule 3: Execution of hidden binary image
        size_t last_slash = path.find_last_of('/');
        if (last_slash != std::string::npos && last_slash + 1 < path.size()) {
            if (path[last_slash + 1] == '.' && 
                path.substr(last_slash + 1) != "." && 
                path.substr(last_slash + 1) != "..") {
                reason = "Execution of hidden binary image: " + path;
                return true;
            }
        }

        return false;
    }
};

void run_telemetry_loop(int fd) {
    std::cout << "[DAEMON] Telemetry thread started. Listening for kernel events...\n";

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;

    while (!g_shutdown) {
        int ready = poll(&pfd, 1, 250);
        if (ready < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (ready > 0 && (pfd.revents & POLLIN)) {
            kshield_telemetry ev;
            while (::read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
                std::string threat_reason;
                bool flagged = ThreatClassifier::isThreat(ev, threat_reason);

                std::cout << "[EVENT] PID: " << ev.pid 
                          << " | PPID: " << ev.ppid 
                          << " | UID: " << ev.uid 
                          << " | COMM: " << ev.comm 
                          << " | PATH: " << ev.target_path << "\n";

                if (flagged) {
                    std::cerr << "\n=======================================================\n"
                              << " [!] THREAT ALERT: MALICIOUS EXECUTION IDENTIFIED\n"
                              << "-------------------------------------------------------\n"
                              << " Target PID:   " << ev.pid << "\n"
                              << " Parent PID:   " << ev.ppid << "\n"
                              << " User ID:      " << ev.uid << "\n"
                              << " Path:         " << ev.target_path << "\n"
                              << " Trigger:      " << threat_reason << "\n"
                              << " Mitigation:   Dispatching SIGKILL immediately...\n"
                              << "=======================================================\n\n";

                    kill(ev.pid, SIGKILL);
                }
            }
        }
    }
}

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    std::cout << "[DAEMON] Initializing K-Shield Threat Detection Daemon...\n";

    SafeDescriptor dev_fd(::open(KSHIELD_DEVICE_PATH, O_RDONLY | O_NONBLOCK));
    if (!dev_fd.valid()) {
        std::cerr << "[DAEMON: ERROR] Failed to access " << KSHIELD_DEVICE_PATH << "\n"
                  << "Run: sudo insmod kshield_driver.ko && sudo chmod 666 /dev/kshield\n";
        return 1;
    }

    std::thread worker(run_telemetry_loop, dev_fd.get());

    while (!g_shutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\n[DAEMON] Shutting down cleanly...\n";
    worker.join();
    return 0;
}
