#pragma once
#include <cstdint>
#include <functional>

namespace avm::input {

// A single synthesized Android touch event sent to the guest.
struct TouchEvent {
    enum class Type : uint8_t { DOWN, MOVE, UP };
    Type     type;
    int32_t  slot;   // multitouch slot (0-9)
    float    x, y;   // normalized [0..1] relative to screen
};

// A synthesized Android key event (for gamepad / keyboard passthrough).
struct KeyEvent {
    enum class Action : uint8_t { DOWN, UP };
    Action   action;
    int32_t  android_keycode;  // e.g. AKEYCODE_BUTTON_A
};

// Callback types the bridge fires when events are ready to inject.
using TouchCallback = std::function<void(const TouchEvent&)>;
using KeyCallback   = std::function<void(const KeyEvent&)>;

// InputBridge — translates host SDL2 events into Android touch/key events
// and dispatches them via ADB or virtio-input depending on transport.
class InputBridge {
public:
    virtual ~InputBridge() = default;

    virtual void set_touch_callback(TouchCallback cb) = 0;
    virtual void set_key_callback(KeyCallback cb)     = 0;

    // Feed a raw SDL_Event from the main loop.
    virtual void handle_sdl_event(const void* sdl_event) = 0;

    // Screen dimensions needed to convert pixel coords → normalized.
    virtual void set_screen_size(int width, int height) = 0;
};

} // namespace avm::input
