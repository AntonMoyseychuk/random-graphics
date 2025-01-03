#pragma once

#include <array>
#include <cstdint>


enum class KeyboardKey : uint8_t
{
    KEY_SPACE,
    KEY_APOSTROPHE, /* ' */
    KEY_COMMA,      /* , */
    KEY_MINUS,      /* - */
    KEY_PERIOD,     /* . */
    KEY_SLASH,      /* / */
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_SEMICOLON,  /* ; */
    KEY_EQUAL,      /* = */
    KEY_A,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,
    KEY_LEFT_BRACKET,  /* [ */
    KEY_BACKSLASH,     /* \ */
    KEY_RIGHT_BRACKET, /* ] */
    KEY_GRAVE_ACCENT,  /* ` */
    KEY_ESCAPE,
    KEY_ENTER,
    KEY_TAB,
    KEY_BACKSPACE,
    KEY_INSERT,
    KEY_DELETE,
    KEY_RIGHT,
    KEY_LEFT,
    KEY_DOWN,
    KEY_UP,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_HOME,
    KEY_END,
    KEY_CAPS_LOCK,
    KEY_SCROLL_LOCK,
    KEY_NUM_LOCK,
    KEY_PRINT_SCREEN,
    KEY_PAUSE,
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_F13,
    KEY_F14,
    KEY_F15,
    KEY_F16,
    KEY_F17,
    KEY_F18,
    KEY_F19,
    KEY_F20,
    KEY_F21,
    KEY_F22,
    KEY_F23,
    KEY_F24,
    KEY_F25,
    KEY_KP_0,
    KEY_KP_1,
    KEY_KP_2,
    KEY_KP_3,
    KEY_KP_4,
    KEY_KP_5,
    KEY_KP_6,
    KEY_KP_7,
    KEY_KP_8,
    KEY_KP_9,
    KEY_KP_DECIMAL,
    KEY_KP_DIVIDE,
    KEY_KP_MULTIPLY,
    KEY_KP_SUBTRACT,
    KEY_KP_ADD,
    KEY_KP_ENTER,
    KEY_KP_EQUAL,
    KEY_LEFT_SHIFT,
    KEY_LEFT_CONTROL,
    KEY_LEFT_ALT,
    KEY_LEFT_SUPER,
    KEY_RIGHT_SHIFT,
    KEY_RIGHT_CONTROL,
    KEY_RIGHT_ALT,
    KEY_RIGHT_SUPER,
    KEY_MENU,

    KEY_COUNT
};


enum class MouseButton : uint8_t
{
    BUTTON_LEFT,
    BUTTON_RIGHT,
    BUTTON_MIDDLE,

    BUTTON_COUNT
};


enum class KeyState : uint8_t
{
    STATE_RELEASED,
    STATE_PRESSED,
    STATE_HOLD,

    STATE_COUNT
};

enum class MouseButtonState : uint8_t
{
    STATE_RELEASED,
    STATE_PRESSED,
    STATE_HOLD,

    STATE_COUNT
};


struct CursorPositon
{
    float x = 0.0f;
    float y = 0.0f;
};


#define DECLARE_MOUSE_BUTTON_EVENT_BODY(EventName)          \
public:                                                     \
    EventName(int32_t button)                               \
        : m_button(button) {}                               \
                                                            \
    int32_t GetButton() const noexcept { return m_button; } \
                                                            \
private:                                                    \
    int32_t m_button;


#define DECLARE_KEYBOARD_EVENT_BODY(EventName)                  \
public:                                                         \
    EventName(int32_t key, int32_t scancode)                    \
        : m_key(key), m_scancode(scancode) {}                   \
                                                                \
    int32_t GetKey() const noexcept { return m_key; }           \
    int32_t GetScancode() const noexcept { return m_scancode; } \
                                                                \
private:                                                        \
    int32_t m_key;                                              \
    int32_t m_scancode;



struct EventMousePressed
{
    DECLARE_MOUSE_BUTTON_EVENT_BODY(EventMousePressed)
};


struct EventMouseReleased
{
    DECLARE_MOUSE_BUTTON_EVENT_BODY(EventMouseReleased)
};


struct EventMouseHold
{
    DECLARE_MOUSE_BUTTON_EVENT_BODY(EventMouseHold)
};


struct EventCursorLeaved
{
};


struct EventCursorEntered
{
};


struct EventKeyPressed
{
    DECLARE_KEYBOARD_EVENT_BODY(EventKeyPressed)
};


struct EventKeyReleased
{
    DECLARE_KEYBOARD_EVENT_BODY(EventKeyReleased)
};


struct EventKeyHold
{
    DECLARE_KEYBOARD_EVENT_BODY(EventKeyHold)
};


struct EventCursorMoved
{
public:
    EventCursorMoved(float x, float y)
        : m_xpos(x), m_ypos(y) {}

    float GetX() const noexcept { return m_xpos; }
    float GetY() const noexcept { return m_ypos; }

private:
    float m_xpos;
    float m_ypos;
};


struct EventMouseWheel
{
public:
    EventMouseWheel(float xoffset, float yoffset)
        : m_xoffset(xoffset), m_yoffset(yoffset) {}

    float GetDX() const noexcept { return m_xoffset; }
    float GetDY() const noexcept { return m_yoffset; }

private:
    float m_xoffset;
    float m_yoffset;
};


class Window;


class Input
{
    friend class Window;

public:
    Input(Window* pWindow);

    KeyState GetKeyState(KeyboardKey key) const noexcept;
    MouseButtonState GetMouseButtonState(MouseButton button) const noexcept;
    
    const CursorPositon& GetCursorPosition() const noexcept { return m_currCursorPosition; }
    float GetCursorDx() const noexcept { return m_currCursorPosition.x - m_prevCursorPosition.x; }
    float GetCursorDy() const noexcept { return m_currCursorPosition.y - m_prevCursorPosition.y; }
    
private:
    Input() = default;

    bool BindWindow(Window* pWindow) noexcept;

    bool IsInitialized() const noexcept { return m_pBoundWindow != nullptr; }

private:
    void OnKeyEvent(KeyboardKey key, KeyState state) noexcept;
    void OnMouseButtonEvent(MouseButton button, MouseButtonState state) noexcept;
    void OnMouseMoveEvent(double xpos, double ypos) noexcept;

private:
    std::array<KeyState, static_cast<size_t>(KeyboardKey::KEY_COUNT)> m_keyStates;

    CursorPositon m_prevCursorPosition;
    CursorPositon m_currCursorPosition;

    Window* m_pBoundWindow = nullptr;
    
    std::array<MouseButtonState, static_cast<size_t>(MouseButton::BUTTON_COUNT)> m_mouseButtonStates;
};