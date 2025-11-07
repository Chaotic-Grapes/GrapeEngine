/* Start Header *****************************************************************/
/*!
\file   Input.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   3rd October 2025
\brief
Declares the Input class for handling keyboard and mouse input events through
GLFW. Provides static functions for checking input states, managing window
events, and accessing system specifications. The class uses callback-based
event handling and maintains internal state tracking for keys and mouse.

Features:
- Static input state checking (key pressed/down/up, mouse buttons)
- Mouse position and scroll tracking
- Window resize event handling
- GLFW callback management and error handling
- OpenGL system specification reporting
- Message system integration for input events
*/
/* End Header *******************************************************************/

#ifndef INPUT_H
#define INPUT_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <unordered_map>

namespace Engine { class Application; } // Forward declaration for friend class

/*!
\class Input
\brief Static input management class for keyboard and mouse events.

Handles all input-related functionality through GLFW, including keyboard
and mouse state tracking, window events, and system information reporting.
All functions are static as this is a singleton-style utility class.
*/
class Input {
public:
    /*!
    \brief Initialize the input system with a GLFW window.
    \param pWin Pointer to the GLFW window to associate with input handling.
    
    Must be called once at startup before using any other input functions.
    Stores the window reference and gets initial window dimensions.
    */
    static void Initialize(GLFWwindow* pWin);

    // Gracefully detach from GLFW window; guards will avoid further GLFW calls
    static void Shutdown();

    /*!
    \brief Check if a key is currently being pressed.
    \param key The key code to check (use KEY_* constants).
    \return True if the key is currently pressed, false otherwise.
    
    Checks the current frame's key press state from internal tracking.
    */
    static bool IsKeyPressed(int key);

    /*!
    \brief Check if a key is currently held down.
    \param key The key code to check (use KEY_* constants).
    \return True if the key is currently down, false otherwise.
    
    Directly queries GLFW for the current key state.
    */
    static bool IsKeyDown(int key);

    /*!
    \brief Check if a key was just released this frame.
    \param key The key code to check (use KEY_* constants).
    \return True if the key was released this frame, false otherwise.
    
    Uses internal state tracking to detect key release events.
    */
    static bool IsKeyUp(int key);

    /*!
    \brief Check if a mouse button is currently pressed.
    \param button The mouse button to check (use MOUSE_* constants).
    \return True if the button is currently pressed, false otherwise.
    
    Directly queries GLFW for the current mouse button state.
    */
    static bool IsMousePressed(int button);

    /*!
    \brief Check if a mouse button is currently held down.
    \param button The mouse button to check (use MOUSE_* constants).
    \return True if the button is currently held down, false otherwise.

    This is equivalent to IsMousePressed(), but is intended for continuous
    checks such as dragging or camera movement.
    */
    static bool IsMouseDown(int button);

    // Check if a mouse button was just released this frame
    static bool IsMouseUp(int button);

    /*!
    \brief Get the current mouse cursor position.
    \param xPos Reference to store the X coordinate.
    \param yPos Reference to store the Y coordinate.
    
    Retrieves the current mouse position in window coordinates.
    */
    static void GetMousePosition(double& xPos, double& yPos);

    /*!
    \brief Get the current mouse X coordinate.
    \return The current X position of the mouse cursor.
    */
    static double GetMouseX();

    /*!
    \brief Get the current mouse Y coordinate.
    \return The current Y position of the mouse cursor.
    */
    static double GetMouseY();

    /*!
    \brief Get the current window width.
    \return The width of the window in pixels.
    */
    static int GetWindowWidth() { return m_windowWidth; }

    /*!
    \brief Get the current window height.
    \return The height of the window in pixels.
    */
    static int GetWindowHeight() { return m_windowHeight; }

    /*!
    \brief Get the current horizontal scroll offset.
    \return The X scroll offset from the last scroll event.
    */
    static double GetScrollX() { return m_scrollX; }

    /*!
    \brief Get the current vertical scroll offset.
    \return The Y scroll offset from the last scroll event.
    */
    static double GetScrollY() { return m_scrollY; }

    /*!
    \brief Set up all GLFW event callbacks for input handling.
    
    Registers callback functions for keyboard, mouse, cursor position,
    scroll, and window resize events with GLFW.
    */
    static void SetupEventCallbacks();

    /*!
    \brief GLFW error callback function.
    \param error The error code from GLFW.
    \param description Text description of the error.
    
    Called automatically by GLFW when an error occurs. Logs the error
    message using the engine's logging system.
    */
    static void ErrorCallback(int error, char const* description);

    /*!
    \brief Print OpenGL system specifications to the log.
    
    Outputs detailed information about the graphics system including
    GPU vendor, renderer, OpenGL version, limits, and capabilities.
    Useful for debugging and system compatibility checking.
    */
    static void PrintSpecs();

private:
    friend class Engine::Application; ///< Allow Application class to access private members

    static GLFWwindow* m_window; ///< Pointer to the GLFW window for input handling

    static std::unordered_map<int, bool> m_keyDown;    ///< Tracks keys currently held down
    static std::unordered_map<int, bool> m_keyPressed; ///< Tracks keys pressed this frame
    static std::unordered_map<int, bool> m_keyUp;      ///< Tracks keys released this frame

    // Mouse button state tracking
    static std::unordered_map<int, bool> m_mouseDown;    // Mouse buttons currently held down
    static std::unordered_map<int, bool> m_mousePressed; // Mouse buttons pressed this frame
    static std::unordered_map<int, bool> m_mouseUp;      // Mouse buttons released this frame

    static int m_windowWidth;  ///< Current window width in pixels
    static int m_windowHeight; ///< Current window height in pixels
    static double m_scrollX;   ///< Horizontal scroll offset from last scroll event
    static double m_scrollY;   ///< Vertical scroll offset from last scroll event

    /*!
    \brief GLFW window resize callback.
    \param pWin Pointer to the GLFW window that was resized.
    \param width New window width in pixels.
    \param height New window height in pixels.
    
    Updates internal window dimension tracking when the window is resized.
    */
    static void _windowSizeCallback(GLFWwindow* pWin, int width, int height);

    /*!
    \brief GLFW keyboard input callback.
    \param pWin Pointer to the GLFW window that received the input.
    \param key The keyboard key that was pressed or released.
    \param scancode The system-specific scancode of the key.
    \param action GLFW_PRESS, GLFW_RELEASE or GLFW_REPEAT.
    \param mod Bit field describing which modifier keys were held down.
    
    Handles keyboard events and updates internal key state tracking.
    Broadcasts key events through the message system.
    */
    static void _keyCallback(GLFWwindow* pWin, int key, int scancode, int action, int mod);

    /*!
    \brief GLFW mouse button callback.
    \param pWin Pointer to the GLFW window that received the input.
    \param button The mouse button that was pressed or released.
    \param action GLFW_PRESS or GLFW_RELEASE.
    \param mod Bit field describing which modifier keys were held down.
    
    Handles mouse button events. Currently not implemented.
    */
    static void _mouseButtonCallback(GLFWwindow* pWin, int button, int action, int mod);

    /*!
    \brief GLFW cursor position callback.
    \param pWin Pointer to the GLFW window that received the input.
    \param xPos The new cursor x-coordinate, relative to the left edge of the content area.
    \param yPos The new cursor y-coordinate, relative to the top edge of the content area.
    
    Handles mouse cursor movement events. Currently not implemented.
    */
    static void _mousePosCallback(GLFWwindow* pWin, double xPos, double yPos);

    /*!
    \brief GLFW scroll callback.
    \param pWin Pointer to the GLFW window that received the input.
    \param xOffset The scroll offset along the x-axis.
    \param yOffset The scroll offset along the y-axis.
    
    Handles mouse wheel scroll events and updates internal scroll tracking.
    */
    static void _mouseScrollCallback(GLFWwindow* pWin, double xOffset, double yOffset);

    /*!
    \brief Process input events and update internal state.
    
    Clears frame-specific input state and polls GLFW events.
    Should be called once per frame.
    */
    static void _processInput();

    static void _fileDropCallback(GLFWwindow* pWin, int count, const char** paths);
    static void _windowFocusCallback(GLFWwindow* pWin, int focused);
};

// Action constants
constexpr int PRESS = GLFW_PRESS;
constexpr int REPEAT = GLFW_REPEAT;
constexpr int RELEASE = GLFW_RELEASE;

// Keyboard constants: letters
constexpr int KEY_A = GLFW_KEY_A;
constexpr int KEY_B = GLFW_KEY_B;
constexpr int KEY_C = GLFW_KEY_C;
constexpr int KEY_D = GLFW_KEY_D;
constexpr int KEY_E = GLFW_KEY_E;
constexpr int KEY_F = GLFW_KEY_F;
constexpr int KEY_G = GLFW_KEY_G;
constexpr int KEY_H = GLFW_KEY_H;
constexpr int KEY_I = GLFW_KEY_I;
constexpr int KEY_J = GLFW_KEY_J;
constexpr int KEY_K = GLFW_KEY_K;
constexpr int KEY_L = GLFW_KEY_L;
constexpr int KEY_M = GLFW_KEY_M;
constexpr int KEY_N = GLFW_KEY_N;
constexpr int KEY_O = GLFW_KEY_O;
constexpr int KEY_P = GLFW_KEY_P;
constexpr int KEY_Q = GLFW_KEY_Q;
constexpr int KEY_R = GLFW_KEY_R;
constexpr int KEY_S = GLFW_KEY_S;
constexpr int KEY_T = GLFW_KEY_T;
constexpr int KEY_U = GLFW_KEY_U;
constexpr int KEY_V = GLFW_KEY_V;
constexpr int KEY_W = GLFW_KEY_W;
constexpr int KEY_X = GLFW_KEY_X;
constexpr int KEY_Y = GLFW_KEY_Y;
constexpr int KEY_Z = GLFW_KEY_Z;

// Keyboard constants: numbers
constexpr int KEY_0 = GLFW_KEY_0;
constexpr int KEY_1 = GLFW_KEY_1;
constexpr int KEY_2 = GLFW_KEY_2;
constexpr int KEY_3 = GLFW_KEY_3;
constexpr int KEY_4 = GLFW_KEY_4;
constexpr int KEY_5 = GLFW_KEY_5;
constexpr int KEY_6 = GLFW_KEY_6;
constexpr int KEY_7 = GLFW_KEY_7;
constexpr int KEY_8 = GLFW_KEY_8;
constexpr int KEY_9 = GLFW_KEY_9;

// Keyboard constants: function keys
constexpr int KEY_F1 = GLFW_KEY_F1;
constexpr int KEY_F2 = GLFW_KEY_F2;
constexpr int KEY_F3 = GLFW_KEY_F3;
constexpr int KEY_F4 = GLFW_KEY_F4;
constexpr int KEY_F5 = GLFW_KEY_F5;
constexpr int KEY_F6 = GLFW_KEY_F6;
constexpr int KEY_F7 = GLFW_KEY_F7;
constexpr int KEY_F8 = GLFW_KEY_F8;
constexpr int KEY_F9 = GLFW_KEY_F9;
constexpr int KEY_F10 = GLFW_KEY_F10;
constexpr int KEY_F11 = GLFW_KEY_F11;
constexpr int KEY_F12 = GLFW_KEY_F12;

// Keyboard constants: special keys
constexpr int KEY_SPACE = GLFW_KEY_SPACE;
constexpr int KEY_APOSTROPHE = GLFW_KEY_APOSTROPHE;       // '
constexpr int KEY_COMMA = GLFW_KEY_COMMA;                 // ,
constexpr int KEY_MINUS = GLFW_KEY_MINUS;                 // -
constexpr int KEY_PERIOD = GLFW_KEY_PERIOD;               // .
constexpr int KEY_SLASH = GLFW_KEY_SLASH;                 // /
constexpr int KEY_SEMICOLON = GLFW_KEY_SEMICOLON;         // ;
constexpr int KEY_EQUAL = GLFW_KEY_EQUAL;                 // =
constexpr int KEY_LEFT_BRACKET = GLFW_KEY_LEFT_BRACKET;   // [
constexpr int KEY_BACKSLASH = GLFW_KEY_BACKSLASH;         // '\'
constexpr int KEY_RIGHT_BRACKET = GLFW_KEY_RIGHT_BRACKET; // ]
constexpr int KEY_GRAVE_ACCENT = GLFW_KEY_GRAVE_ACCENT;   // `
constexpr int KEY_ESCAPE = GLFW_KEY_ESCAPE;
constexpr int KEY_ENTER = GLFW_KEY_ENTER;
constexpr int KEY_TAB = GLFW_KEY_TAB;
constexpr int KEY_BACKSPACE = GLFW_KEY_BACKSPACE;
constexpr int KEY_INSERT = GLFW_KEY_INSERT;
constexpr int KEY_DELETE = GLFW_KEY_DELETE;

// Keyboard constants: arrow keys
constexpr int KEY_RIGHT = GLFW_KEY_RIGHT;
constexpr int KEY_LEFT = GLFW_KEY_LEFT;
constexpr int KEY_DOWN = GLFW_KEY_DOWN;
constexpr int KEY_UP = GLFW_KEY_UP;

// Keyboard constants: modifiers
constexpr int KEY_LEFT_SHIFT = GLFW_KEY_LEFT_SHIFT;
constexpr int KEY_LEFT_CONTROL = GLFW_KEY_LEFT_CONTROL;
constexpr int KEY_LEFT_ALT = GLFW_KEY_LEFT_ALT;
constexpr int KEY_LEFT_SUPER = GLFW_KEY_LEFT_SUPER;      // Windows/Command key
constexpr int KEY_RIGHT_SHIFT = GLFW_KEY_RIGHT_SHIFT;
constexpr int KEY_RIGHT_CONTROL = GLFW_KEY_RIGHT_CONTROL;
constexpr int KEY_RIGHT_ALT = GLFW_KEY_RIGHT_ALT;
constexpr int KEY_RIGHT_SUPER = GLFW_KEY_RIGHT_SUPER;    // Windows/Command key

// Keyboard constants: navigation
constexpr int KEY_PAGE_UP = GLFW_KEY_PAGE_UP;
constexpr int KEY_PAGE_DOWN = GLFW_KEY_PAGE_DOWN;
constexpr int KEY_HOME = GLFW_KEY_HOME;
constexpr int KEY_END = GLFW_KEY_END;
constexpr int KEY_CAPS_LOCK = GLFW_KEY_CAPS_LOCK;

// Mouse button constants
constexpr int MOUSE_LEFT = GLFW_MOUSE_BUTTON_LEFT;
constexpr int MOUSE_RIGHT = GLFW_MOUSE_BUTTON_RIGHT;
constexpr int MOUSE_MIDDLE = GLFW_MOUSE_BUTTON_MIDDLE;

#endif