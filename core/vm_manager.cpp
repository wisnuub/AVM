#include "avm/core/vm_manager.h"
#include "avm/core/config.h"
#include "avm/core/platform.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <cstring>
#include <array>

#if AVM_OS_WINDOWS
#  include <windows.h>
#  include <winsock2.h>
#  pragma comment(lib, "ws2_32.lib")
#else
#  include <unistd.h>
#  include <sys/wait.h>
#  include <sys/socket.h>
#  include <sys/stat.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <signal.h>
#endif

namespace avm {

// ============================================================
//  Construction / Destruction
// ============================================================

VmManager::VmManager(const Config& config)
    : config_(config) {}

VmManager::~VmManager() {
    if (is_running()) stop();
    qmp_disconnect();
}

// ============================================================
//  Public API
// ============================================================

bool VmManager::initialize() {
    std::cout << "[VmManager] Initializing...\n";

    if (!validate_image()) {
        std::cerr << "[VmManager] Image validation failed.\n";
        return false;
    }

    if (config_.hardware_accel) {
        if (!detect_hypervisor()) {
            std::cerr << "[VmManager] WARNING: No hardware hypervisor found.\n"
                      << "            Falling back to software emulation (very slow).\n";
            config_.hardware_accel = false;
            active_hypervisor_ = HypervisorBackend::None;
        }
    }

    std::cout << "[VmManager] Initialization complete.\n";
    return true;
}

int VmManager::run() {
    std::string qemu_bin = find_qemu_binary();
    if (qemu_bin.empty()) {
        std::cerr << "[VmManager] QEMU not found. Install it:\n"
#if AVM_OS_MACOS
                  << "            brew install qemu\n"
                  << "  Note: On Apple Silicon, use qemu-system-aarch64 (ARM64 guest).\n"
#elif AVM_OS_WINDOWS
                  << "            https://www.qemu.org/download/#windows\n"
#else
                  << "            sudo apt install qemu-system-x86 (x86)\n"
                  << "            sudo apt install qemu-system-arm  (ARM64)\n"
#endif
                  ;
        return 1;
    }
    std::cout << "[VmManager] Using QEMU: " << qemu_bin << "\n";

    auto args = build_qemu_args();

    std::cout << "[VmManager] Command line:\n  " << qemu_bin;
    for (auto& a : args) std::cout << " " << a;
    std::cout << "\n\n";

    if (!launch_qemu(qemu_bin, args)) {
        std::cerr << "[VmManager] Failed to launch QEMU.\n";
        return 1;
    }

    running_ = true;
    std::cout << "[VmManager] QEMU started.\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    if (qmp_connect()) {
        std::cout << "[VmManager] QMP connected on port " << qmp_port_ << ".\n";
        qmp_execute("{\"execute\":\"qmp_capabilities\"}");
    } else {
        std::cerr << "[VmManager] QMP connection failed (non-fatal).\n";
    }

    if (config_.enable_adb) {
        std::cout << "[VmManager] Waiting for ADB (port " << config_.adb_port << ")...\n";
        if (wait_for_adb(60))
            std::cout << "[VmManager] ADB ready.\n";
        else
            std::cerr << "[VmManager] ADB timeout (VM may still be booting).\n";
    }

    std::cout << "[VmManager] VM running. Ctrl+C to stop.\n";
    wait_for_exit();
    running_ = false;
    std::cout << "[VmManager] QEMU exited.\n";
    return 0;
}

void VmManager::stop() {
    if (!is_running()) return;
    if (qmp_sock_ >= 0) {
        qmp_execute("{\"execute\":\"quit\"}");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    kill_process();
    running_ = false;
}

bool VmManager::snapshot(const std::string& name) {
    if (!is_running() || qmp_sock_ < 0) return false;
    std::string cmd = "{\"execute\":\"savevm\",\"arguments\":{\"name\":\"" + name + "\"}}";
    return qmp_execute(cmd);
}

bool VmManager::restore_snapshot(const std::string& name) {
    if (!is_running() || qmp_sock_ < 0) return false;
    std::string cmd = "{\"execute\":\"loadvm\",\"arguments\":{\"name\":\"" + name + "\"}}";
    return qmp_execute(cmd);
}

bool VmManager::is_running() const {
#if AVM_OS_WINDOWS
    if (qemu_process_ == INVALID_HANDLE_VALUE) return false;
    DWORD exit_code;
    GetExitCodeProcess(qemu_process_, &exit_code);
    return exit_code == STILL_ACTIVE;
#else
    if (qemu_pid_ <= 0) return false;
    return (waitpid(qemu_pid_, nullptr, WNOHANG) == 0);
#endif
}

// ============================================================
//  Image Validation
// ============================================================

bool VmManager::validate_image() {
    if (config_.system_image_path.empty()) {
        std::cerr << "[VmManager] No system image (--image).\n";
        return false;
    }
    std::ifstream f(config_.system_image_path);
    if (!f.good()) {
        std::cerr << "[VmManager] Image not found: " << config_.system_image_path << "\n";
        return false;
    }
    f.seekg(0, std::ios::end);
    auto size = f.tellg();
    if (size <= 0) {
        std::cerr << "[VmManager] Image appears empty.\n";
        return false;
    }
    std::cout << "[VmManager] Image OK: " << config_.system_image_path
              << " (" << (size / 1024 / 1024) << " MB)\n";
    return true;
}

// ============================================================
//  Hypervisor Detection
// ============================================================

bool VmManager::detect_hypervisor() {
#if AVM_OS_WINDOWS
    // Try AEHD first
    HANDLE aehd = CreateFileA("\\\\.\\aehd",
                               GENERIC_READ | GENERIC_WRITE, 0,
                               nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (aehd != INVALID_HANDLE_VALUE) {
        CloseHandle(aehd);
        active_hypervisor_ = HypervisorBackend::AEHD;
        std::cout << "[VmManager] Hypervisor: AEHD\n";
        return true;
    }
    HMODULE whvdll = LoadLibraryA("WinHvPlatform.dll");
    if (whvdll) {
        FreeLibrary(whvdll);
        active_hypervisor_ = HypervisorBackend::WHPX;
        std::cout << "[VmManager] Hypervisor: WHPX\n";
        return true;
    }
    std::cerr << "[VmManager] No hypervisor on Windows.\n"
              << "  Enable 'Windows Hypervisor Platform' in Windows Features.\n";
    return false;

#elif AVM_OS_MACOS
    // Hypervisor.framework is always present on macOS 11+.
    // The binary must be signed with com.apple.security.hypervisor entitlement
    // (see avm.entitlements and docs/building.md).
    active_hypervisor_ = HypervisorBackend::HVF;
    std::cout << "[VmManager] Hypervisor: Apple Hypervisor.framework";
#  if AVM_ARCH_ARM64
    std::cout << " (Apple Silicon — ARM64 guest required)";
#  else
    std::cout << " (Intel Mac — x86_64 guest)";
#  endif
    std::cout << "\n";
    return true;

#else // Linux
    std::ifstream kvm("/dev/kvm");
    if (kvm.good()) {
        active_hypervisor_ = HypervisorBackend::KVM;
        std::cout << "[VmManager] Hypervisor: KVM\n";
        return true;
    }
    std::cerr << "[VmManager] /dev/kvm not accessible.\n"
              << "  sudo modprobe kvm_intel  (or kvm_amd)\n"
              << "  sudo usermod -aG kvm $USER\n";
    return false;
#endif
}

// ============================================================
//  QEMU Binary Resolution
// ============================================================

std::string VmManager::find_qemu_binary() {
    // On Apple Silicon we need the ARM64 QEMU binary.
    // On all other platforms we use the x86_64 binary.
#if AVM_OS_MACOS && AVM_ARCH_ARM64
    const std::vector<std::string> candidates = {
        "qemu-system-aarch64",
        "/opt/homebrew/bin/qemu-system-aarch64",
        "/usr/local/bin/qemu-system-aarch64",
    };
#elif AVM_OS_WINDOWS
    const std::vector<std::string> candidates = {
        "qemu-system-x86_64.exe",
        "C:\\Program Files\\qemu\\qemu-system-x86_64.exe",
        "C:\\qemu\\qemu-system-x86_64.exe",
    };
#else
    const std::vector<std::string> candidates = {
        "qemu-system-x86_64",
        "/usr/bin/qemu-system-x86_64",
        "/usr/local/bin/qemu-system-x86_64",
        "/opt/homebrew/bin/qemu-system-x86_64",
        "/opt/local/bin/qemu-system-x86_64",
    };
#endif

    for (auto& path : candidates) {
#if AVM_OS_WINDOWS
        if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES)
            return path;
#else
        struct stat st;
        if (stat(path.c_str(), &st) == 0 && (st.st_mode & S_IXUSR))
            return path;
#endif
    }
    return {};
}

// ============================================================
//  QEMU Argument Builder
// ============================================================

std::vector<std::string> VmManager::build_qemu_args() {
    std::vector<std::string> args;

    // --- Acceleration ---
    if (config_.hardware_accel) {
#if AVM_OS_WINDOWS
        if (active_hypervisor_ == HypervisorBackend::AEHD) {
            args.push_back("-accel"); args.push_back("aehd");
        } else {
            args.push_back("-accel"); args.push_back("whpx");
        }
#elif AVM_OS_MACOS
        args.push_back("-accel"); args.push_back("hvf");
        args.push_back("-cpu");   args.push_back("host");
#else
        args.push_back("-accel"); args.push_back("kvm");
        args.push_back("-cpu");   args.push_back("host");
#endif
    } else {
        args.push_back("-accel"); args.push_back("tcg");
        args.push_back("-cpu");   args.push_back("max");
    }

    // --- Machine type ---
    // Apple Silicon uses virt (ARM64); x86 uses q35
#if AVM_OS_MACOS && AVM_ARCH_ARM64
    args.push_back("-machine"); args.push_back("virt,highmem=off");
#else
    args.push_back("-machine"); args.push_back("q35");
#endif

    // --- Memory ---
    args.push_back("-m");
    args.push_back(std::to_string(config_.memory_mb) + "M");

    // --- vCPUs ---
    args.push_back("-smp");
    args.push_back(std::to_string(config_.vcpu_cores));

    // --- System image ---
    args.push_back("-drive");
    args.push_back("file=" + config_.system_image_path +
                   ",format=raw,if=virtio,readonly=on");

    // --- Data partition ---
    if (!config_.data_partition_path.empty()) {
        args.push_back("-drive");
        args.push_back("file=" + config_.data_partition_path +
                       ",format=raw,if=virtio");
    }

    // --- GPU ---
    if (config_.gpu_backend != GpuBackend::Software) {
        args.push_back("-device"); args.push_back("virtio-gpu-gl-pci");
        args.push_back("-display"); args.push_back("sdl,gl=on");
    } else {
        args.push_back("-device"); args.push_back("virtio-vga");
        args.push_back("-display"); args.push_back("sdl");
    }

    // --- Input ---
    args.push_back("-device"); args.push_back("virtio-mouse-pci");
    args.push_back("-device"); args.push_back("virtio-keyboard-pci");
    args.push_back("-device"); args.push_back("virtio-tablet-pci");
    args.push_back("-usb");

    // --- Network + ADB forward ---
    args.push_back("-netdev");
    args.push_back("user,id=net0,hostfwd=tcp::" +
                   std::to_string(config_.adb_port) + "-:5555");
    args.push_back("-device"); args.push_back("virtio-net-pci,netdev=net0");

    // --- Audio ---
    args.push_back("-audiodev"); args.push_back("none,id=audio0");

    // --- QMP ---
    args.push_back("-qmp");
    args.push_back("tcp:127.0.0.1:" + std::to_string(qmp_port_) +
                   ",server=on,wait=off");

    // --- Serial / monitor ---
    args.push_back("-serial");  args.push_back("stdio");
    args.push_back("-monitor"); args.push_back("none");
    args.push_back("-nodefaults");

    return args;
}

// ============================================================
//  Process Launch
// ============================================================

bool VmManager::launch_qemu(const std::string& qemu_bin,
                             const std::vector<std::string>& args) {
#if AVM_OS_WINDOWS
    std::ostringstream cmd;
    cmd << "\"" << qemu_bin << "\"";
    for (auto& a : args)
        cmd << (a.find(' ') != std::string::npos ? " \"" + a + "\"" : " " + a);
    std::string cmd_str = cmd.str();
    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(nullptr, cmd_str.data(), nullptr, nullptr,
                        FALSE, 0, nullptr, nullptr, &si, &pi)) {
        std::cerr << "[VmManager] CreateProcess failed: " << GetLastError() << "\n";
        return false;
    }
    qemu_process_ = pi.hProcess;
    qemu_thread_  = pi.hThread;
    return true;
#else
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[VmManager] fork() failed: " << strerror(errno) << "\n";
        return false;
    }
    if (pid == 0) {
        std::vector<const char*> argv;
        argv.push_back(qemu_bin.c_str());
        for (auto& a : args) argv.push_back(a.c_str());
        argv.push_back(nullptr);
        execvp(qemu_bin.c_str(), const_cast<char* const*>(argv.data()));
        std::cerr << "[VmManager] execvp failed: " << strerror(errno) << "\n";
        _exit(1);
    }
    qemu_pid_ = pid;
    std::cout << "[VmManager] QEMU PID: " << qemu_pid_ << "\n";
    return true;
#endif
}

void VmManager::wait_for_exit() {
#if AVM_OS_WINDOWS
    if (qemu_process_ != INVALID_HANDLE_VALUE)
        WaitForSingleObject(qemu_process_, INFINITE);
#else
    if (qemu_pid_ > 0) { int s; waitpid(qemu_pid_, &s, 0); }
#endif
}

void VmManager::kill_process() {
#if AVM_OS_WINDOWS
    if (qemu_process_ != INVALID_HANDLE_VALUE) {
        TerminateProcess(qemu_process_, 0);
        CloseHandle(qemu_process_); CloseHandle(qemu_thread_);
        qemu_process_ = qemu_thread_ = INVALID_HANDLE_VALUE;
    }
#else
    if (qemu_pid_ > 0) {
        kill(qemu_pid_, SIGTERM);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (is_running()) kill(qemu_pid_, SIGKILL);
        waitpid(qemu_pid_, nullptr, 0);
        qemu_pid_ = -1;
    }
#endif
}

// ============================================================
//  QMP
// ============================================================

bool VmManager::qmp_connect() {
#if AVM_OS_WINDOWS
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
    qmp_sock_ = (int)socket(AF_INET, SOCK_STREAM, 0);
#else
    qmp_sock_ = socket(AF_INET, SOCK_STREAM, 0);
#endif
    if (qmp_sock_ < 0) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(qmp_port_);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    for (int i = 0; i < 5; ++i) {
        if (connect(qmp_sock_, (sockaddr*)&addr, sizeof(addr)) == 0) {
            qmp_recv(); // consume greeting
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
#if AVM_OS_WINDOWS
    closesocket(qmp_sock_);
#else
    close(qmp_sock_);
#endif
    qmp_sock_ = -1;
    return false;
}

void VmManager::qmp_disconnect() {
    if (qmp_sock_ < 0) return;
#if AVM_OS_WINDOWS
    closesocket(qmp_sock_); WSACleanup();
#else
    close(qmp_sock_);
#endif
    qmp_sock_ = -1;
}

bool VmManager::qmp_send(const std::string& cmd) {
    if (qmp_sock_ < 0) return false;
    std::string msg = cmd + "\n";
    return send(qmp_sock_, msg.c_str(), (int)msg.size(), 0) > 0;
}

std::string VmManager::qmp_recv() {
    if (qmp_sock_ < 0) return {};
    std::array<char,4096> buf{};
    int n = recv(qmp_sock_, buf.data(), (int)buf.size()-1, 0);
    if (n <= 0) return {};
    buf[n] = '\0';
    return { buf.data(), (size_t)n };
}

bool VmManager::qmp_execute(const std::string& cmd_json) {
    if (!qmp_send(cmd_json)) return false;
    std::string resp = qmp_recv();
    bool ok = resp.find("\"return\"") != std::string::npos;
    if (!ok) std::cerr << "[QMP] " << resp << "\n";
    return ok;
}

// ============================================================
//  ADB
// ============================================================

bool VmManager::wait_for_adb(int timeout_seconds) {
    system(("adb connect 127.0.0.1:" + std::to_string(config_.adb_port)).c_str());
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::seconds(timeout_seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        std::string out;
        if (adb_shell("getprop sys.boot_completed", out)) {
            out.erase(0, out.find_first_not_of(" \t\n\r"));
            out.erase(out.find_last_not_of(" \t\n\r") + 1);
            if (out == "1") return true;
        }
        std::cout << "[ADB] Waiting for Android to boot...\n";
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
    return false;
}

bool VmManager::adb_shell(const std::string& cmd, std::string& output) {
    std::string full = "adb -s 127.0.0.1:" + std::to_string(config_.adb_port) +
                       " shell " + cmd + " 2>/dev/null";
#if AVM_OS_WINDOWS
    FILE* pipe = _popen(full.c_str(), "r");
#else
    FILE* pipe = popen(full.c_str(), "r");
#endif
    if (!pipe) return false;
    std::array<char,256> buf{};
    output.clear();
    while (fgets(buf.data(), (int)buf.size(), pipe)) output += buf.data();
#if AVM_OS_WINDOWS
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return true;
}

} // namespace avm
