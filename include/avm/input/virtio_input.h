#pragma once
#include "input_bridge.h"
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace avm::input {

// VirtioInputTransport — injects input events directly into the guest kernel
// via the virtio-input device, bypassing ADB entirely.
//
// This is the low-latency path: events travel host-kernel → QEMU virtio bus
// → guest kernel evdev driver → Android InputReader.
// Typical latency: 1-3 ms vs 20-50 ms for ADB.
//
// Requires: QEMU launched with -device virtio-input-host-pci or using
// QEMU's built-in multi-touch tablet (-device virtio-tablet-pci).
class VirtioInputTransport {
public:
    struct Config {
        std::string qmp_socket;   // QEMU QMP Unix socket path, e.g. "/tmp/avm-qmp.sock"
        int         screen_w = 1080;
        int         screen_h = 1920;
        int         max_slots = 10;  // multitouch slots
    };

    explicit VirtioInputTransport(const Config& cfg);
    ~VirtioInputTransport();

    bool open();   // open QMP socket and negotiate capabilities
    void close();
    bool is_open() const { return open_; }

    void inject_touch(const TouchEvent& e);
    void inject_key(const KeyEvent& e);

private:
    // evdev event codes we emit (subset of linux/input-event-codes.h)
    static constexpr uint16_t EV_SYN  = 0x00;
    static constexpr uint16_t EV_KEY  = 0x01;
    static constexpr uint16_t EV_ABS  = 0x03;
    static constexpr uint16_t ABS_MT_SLOT       = 0x2f;
    static constexpr uint16_t ABS_MT_TRACKING_ID = 0x39;
    static constexpr uint16_t ABS_MT_POSITION_X  = 0x35;
    static constexpr uint16_t ABS_MT_POSITION_Y  = 0x36;
    static constexpr uint16_t ABS_MT_PRESSURE    = 0x3a;
    static constexpr uint16_t SYN_REPORT         = 0x00;
    static constexpr uint16_t BTN_TOUCH          = 0x14a;

    // Asynchronous write queue so the main thread never blocks on socket I/O.
    struct InputEvent { uint16_t type, code; int32_t value; };

    void     enqueue(InputEvent e);
    void     flush_thread_fn();   // background writer thread
    void     write_event(const InputEvent& e);
    int32_t  next_tracking_id();

    Config              cfg_;
    bool                open_  = false;
    intptr_t            fd_    = -1;
    int32_t             tid_counter_ = 1;

    std::thread                   flush_thread_;
    std::atomic<bool>             running_{ false };
    std::queue<InputEvent>        event_queue_;
    std::mutex                    queue_mutex_;
    std::condition_variable       queue_cv_;

    // Current slot states for tracking IDs
    struct SlotState { int32_t tracking_id = -1; bool active = false; };
    SlotState slots_[10];
};

} // namespace avm::input
