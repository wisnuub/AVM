#pragma once
#include <cstdint>
#include <string>

namespace avm {

enum class InputEventType {
    TouchDown,
    TouchUp,
    TouchMove,
    KeyDown,
    KeyUp,
    MouseMove,
    MouseScroll,
    GamepadButton,
    GamepadAxis
};

struct InputEvent {
    InputEventType type;
    int32_t  x = 0;     // touch X (pixels) or key code
    int32_t  y = 0;     // touch Y (pixels)
    float    value = 0; // axis value, scroll delta
    uint32_t id   = 0;  // touch pointer ID, gamepad button ID
};

/**
 * InputBridge — captures host input events and translates them
 * into Android touch/key events, injected via the virtio-input
 * device or ADB.
 *
 * Also responsible for applying the active keymapper profile:
 * e.g., W key → swipe up gesture, left mouse → tap at (x, y).
 */
class InputBridge {
public:
    virtual ~InputBridge() = default;

    virtual bool initialize(int adb_port) = 0;
    virtual void shutdown() = 0;

    /** Load a keymapper profile from a JSON file. */
    virtual bool load_profile(const std::string& profile_path) = 0;

    /** Inject a pre-built input event directly. */
    virtual void inject(const InputEvent& event) = 0;

    /** Poll and process host input events — call once per frame. */
    virtual void poll() = 0;
};

} // namespace avm
