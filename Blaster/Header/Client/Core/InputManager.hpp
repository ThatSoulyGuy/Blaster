#pragma once

#include "Client/Core/Window.hpp"

namespace Blaster::Client::Core
{
    enum class KeyCode
    {
        SPACE = GLFW_KEY_SPACE,
        APOSTROPHE = GLFW_KEY_APOSTROPHE,
        COMMA = GLFW_KEY_COMMA,
        MINUS = GLFW_KEY_MINUS,
        PERIOD = GLFW_KEY_PERIOD,
        SLASH = GLFW_KEY_SLASH,
        ZERO = GLFW_KEY_0,
        ONE = GLFW_KEY_1,
        TWO = GLFW_KEY_2,
        THREE = GLFW_KEY_3,
        FOUR = GLFW_KEY_4,
        FIVE = GLFW_KEY_5,
        SIX = GLFW_KEY_6,
        SEVEN = GLFW_KEY_7,
        EIGHT = GLFW_KEY_8,
        NINE = GLFW_KEY_9,
        SEMICOLON = GLFW_KEY_SEMICOLON,
        EQUAL = GLFW_KEY_EQUAL,
        A = GLFW_KEY_A,
        B = GLFW_KEY_B,
        C = GLFW_KEY_C,
        D = GLFW_KEY_D,
        E = GLFW_KEY_E,
        F = GLFW_KEY_F,
        G = GLFW_KEY_G,
        H = GLFW_KEY_H,
        I = GLFW_KEY_I,
        J = GLFW_KEY_J,
        K = GLFW_KEY_K,
        L = GLFW_KEY_L,
        M = GLFW_KEY_M,
        N = GLFW_KEY_N,
        O = GLFW_KEY_O,
        P = GLFW_KEY_P,
        Q = GLFW_KEY_Q,
        R = GLFW_KEY_R,
        S = GLFW_KEY_S,
        T = GLFW_KEY_T,
        U = GLFW_KEY_U,
        V = GLFW_KEY_V,
        W = GLFW_KEY_W,
        X = GLFW_KEY_X,
        Y = GLFW_KEY_Y,
        Z = GLFW_KEY_Z,
        LEFT_BRACKET = GLFW_KEY_LEFT_BRACKET,
        BACKSLASH = GLFW_KEY_BACKSLASH,
        RIGHT_BRACKET = GLFW_KEY_RIGHT_BRACKET,
        GRAVE_ACCENT = GLFW_KEY_GRAVE_ACCENT,
        WORLD_1 = GLFW_KEY_WORLD_1,
        WORLD_2 = GLFW_KEY_WORLD_2,
        ESCAPE = GLFW_KEY_ESCAPE,
        ENTER = GLFW_KEY_ENTER,
        TAB = GLFW_KEY_TAB,
        BACKSPACE = GLFW_KEY_BACKSPACE,
        INSERT = GLFW_KEY_INSERT,
        DELETE_KEY = GLFW_KEY_DELETE,
        RIGHT = GLFW_KEY_RIGHT,
        LEFT = GLFW_KEY_LEFT,
        DOWN = GLFW_KEY_DOWN,
        UP = GLFW_KEY_UP,
        PAGE_UP = GLFW_KEY_PAGE_UP,
        PAGE_DOWN = GLFW_KEY_PAGE_DOWN,
        HOME = GLFW_KEY_HOME,
        END = GLFW_KEY_END,
        CAPS_LOCK = GLFW_KEY_CAPS_LOCK,
        SCROLL_LOCK = GLFW_KEY_SCROLL_LOCK,
        NUM_LOCK = GLFW_KEY_NUM_LOCK,
        PRINT_SCREEN = GLFW_KEY_PRINT_SCREEN,
        PAUSE = GLFW_KEY_PAUSE,
        F1 = GLFW_KEY_F1,
        F2 = GLFW_KEY_F2,
        F3 = GLFW_KEY_F3,
        F4 = GLFW_KEY_F4,
        F5 = GLFW_KEY_F5,
        F6 = GLFW_KEY_F6,
        F7 = GLFW_KEY_F7,
        F8 = GLFW_KEY_F8,
        F9 = GLFW_KEY_F9,
        F10 = GLFW_KEY_F10,
        F11 = GLFW_KEY_F11,
        F12 = GLFW_KEY_F12,
        F13 = GLFW_KEY_F13,
        F14 = GLFW_KEY_F14,
        F15 = GLFW_KEY_F15,
        F16 = GLFW_KEY_F16,
        F17 = GLFW_KEY_F17,
        F18 = GLFW_KEY_F18,
        F19 = GLFW_KEY_F19,
        F20 = GLFW_KEY_F20,
        F21 = GLFW_KEY_F21,
        F22 = GLFW_KEY_F22,
        F23 = GLFW_KEY_F23,
        F24 = GLFW_KEY_F24,
        F25 = GLFW_KEY_F25,
        KEYPAD_ZERO = GLFW_KEY_KP_0,
        KEYPAD_ONE = GLFW_KEY_KP_1,
        KEYPAD_TWO = GLFW_KEY_KP_2,
        KEYPAD_THREE = GLFW_KEY_KP_3,
        KEYPAD_FOUR = GLFW_KEY_KP_4,
        KEYPAD_FIVE = GLFW_KEY_KP_5,
        KEYPAD_SIX = GLFW_KEY_KP_6,
        KEYPAD_SEVEN = GLFW_KEY_KP_7,
        KEYPAD_EIGHT = GLFW_KEY_KP_8,
        KEYPAD_NINE = GLFW_KEY_KP_9,
        KEYPAD_DECIMAL = GLFW_KEY_KP_DECIMAL,
        KEYPAD_DIVIDE = GLFW_KEY_KP_DIVIDE,
        KEYPAD_MULTIPLY = GLFW_KEY_KP_MULTIPLY,
        KEYPAD_SUBTRACT = GLFW_KEY_KP_SUBTRACT,
        KEYPAD_ADD = GLFW_KEY_KP_ADD,
        KEYPAD_ENTER = GLFW_KEY_KP_ENTER,
        KEYPAD_EQUAL = GLFW_KEY_KP_EQUAL,
        LEFT_SHIFT = GLFW_KEY_LEFT_SHIFT,
        LEFT_CONTROL = GLFW_KEY_LEFT_CONTROL,
        LEFT_ALT = GLFW_KEY_LEFT_ALT,
        LEFT_SUPER = GLFW_KEY_LEFT_SUPER,
        RIGHT_SHIFT = GLFW_KEY_RIGHT_SHIFT,
        RIGHT_CONTROL = GLFW_KEY_RIGHT_CONTROL,
        RIGHT_ALT = GLFW_KEY_RIGHT_ALT,
        RIGHT_SUPER = GLFW_KEY_RIGHT_SUPER,
        MENU = GLFW_KEY_MENU
    };

    enum class KeyState
    {
        PRESSED,
        HELD,
        RELEASED
    };

    enum class MouseCode
    {
        LEFT = 0,
        MIDDLE = 2,
        RIGHT = 1
    };

    enum class MouseState
    {
        PRESSED,
        HELD,
        RELEASED
    };

    enum class MouseMode
    {
        LOCKED,
        FREE
    };

    inline MouseMode operator!(const MouseMode& mode)
    {
        switch (mode)
        {

        case MouseMode::LOCKED:
            return MouseMode::FREE;

        case MouseMode::FREE:
            return MouseMode::LOCKED;

        default:
            return MouseMode::FREE;
        }
    }

    class InputManager final
    {

    public:

        void Initialize()
        {
            handle = Window::GetInstance().GetHandle();

            glfwSetScrollCallback(handle, ScrollCallback);

            currentKeyStates.resize(GLFW_KEY_LAST + 1, false);
            previousKeyStates.resize(GLFW_KEY_LAST + 1, false);

            currentMouseStates.resize(8, false);
            previousMouseStates.resize(8, false);

            double x, y;

            glfwGetCursorPos(handle, &x, &y);

            mousePosition = Vector<float, 2>{ static_cast<float>(x), static_cast<float>(y) };
        }

        bool GetKeyState(const KeyCode& code, const KeyState& state) const
        {
            const int key = static_cast<int>(code);

            const bool wasPressed = previousKeyStates[key];
            const bool isPressed = currentKeyStates[key];

            switch (state)
            {
            case KeyState::PRESSED:
                return isPressed && !wasPressed;

            case KeyState::HELD:
                return isPressed;

            case KeyState::RELEASED:
                return !isPressed && wasPressed;

            default:
                return false;
            }
        }

        bool GetMouseState(const MouseCode& code, const MouseState& state) const
        {
            const int button = static_cast<int>(code);
            const bool wasPressed = previousMouseStates[button];
            const bool isPressed = currentMouseStates[button];

            switch (state)
            {
            case MouseState::PRESSED:
                return isPressed && !wasPressed;

            case MouseState::HELD:
                return isPressed;

            case MouseState::RELEASED:
                return !isPressed && wasPressed;

            default:
                return false;
            }
        }

        void SetMouseMode(const MouseMode& mode) const
        {
            switch (mode)
            {

            case MouseMode::LOCKED:
                glfwSetInputMode(handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                break;

            case MouseMode::FREE:
                glfwSetInputMode(handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                break;

            default:
                break;
            }
        }

        MouseMode GetMouseMode() const
        {
            switch (glfwGetInputMode(handle, GLFW_CURSOR))
            {

            case GLFW_CURSOR_DISABLED:
                return MouseMode::LOCKED;

            case GLFW_CURSOR_NORMAL:
                return MouseMode::FREE;

            default:
                return MouseMode::FREE;
            }
        }

        Vector<float, 2> GetMousePosition() const
        {
            return mousePosition;
        }

        Vector<float, 2> GetMouseDelta() const
        {
            return mouseDelta;
        }

        float GetScrollDelta() const
        {
            return scrollDelta;
        }

        void Update()
        {
            if (!handle)
                return;

            previousKeyStates = currentKeyStates;
            previousMouseStates = currentMouseStates;

            for (int key = 0; key < static_cast<int>(currentKeyStates.size()); ++key)
                currentKeyStates[key] = glfwGetKey(handle, key) == GLFW_PRESS || glfwGetKey(handle, key) == GLFW_REPEAT;

            for (int button = 0; button < static_cast<int>(currentMouseStates.size()); ++button)
                currentMouseStates[button] = glfwGetMouseButton(handle, button) == GLFW_PRESS;

            double x, y;

            glfwGetCursorPos(handle, &x, &y);

            const Vector<float, 2> newPosition{ static_cast<float>(x), static_cast<float>(y) };

            mouseDelta = newPosition - mousePosition;
            mousePosition = newPosition;
        }

        static InputManager& GetInstance()
        {
            std::call_once(initializationFlag, []()
            {
                instance = std::unique_ptr<InputManager>(new InputManager());
            });

            return *instance;
        }

    private:

        InputManager() = default;

        static void ScrollCallback(GLFWwindow*, double, double offsetY)
        {
            GetInstance().scrollDelta = static_cast<float>(offsetY);
        }

        GLFWwindow* handle = nullptr;

        std::vector<bool> currentKeyStates;
        std::vector<bool> previousKeyStates;
        std::vector<bool> currentMouseStates;
        std::vector<bool> previousMouseStates;

        Vector<float, 2> mousePosition = { 0.0f, 0.0f };
        Vector<float, 2> mouseDelta = { 0.0f, 0.0f };

        float scrollDelta = 0.0f;

        static std::unique_ptr<InputManager> instance;
        static std::once_flag initializationFlag;

    };

    std::unique_ptr<InputManager> InputManager::instance = nullptr;
    std::once_flag InputManager::initializationFlag;
}