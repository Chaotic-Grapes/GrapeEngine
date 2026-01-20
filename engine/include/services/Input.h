/* Start Header *****************************************************************/
/*!
\file   Input.h
\author Foo Rui Qin (70%)
        Muhammad Nur Fadzly Bin Zulkifli (30%)
\par    ruiqin.foo@digipen.edu
        muhammadnurfadzly.b@digipen.edu
\date   26th October 2025
\brief
Declares the Input class for handling keyboard and mouse input events through
GLFW. Provides static functions for checking input states, managing window
events, and accessing system specifications.

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

#include "Export.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <unordered_map>
#include <string>

namespace Engine { class Application; }

// Maximum key and mouse button constants
static constexpr int MAX_KEYS   = GLFW_KEY_LAST + 1;
static constexpr int MAX_MOUSE  = GLFW_MOUSE_BUTTON_LAST + 1;

// Static input management class for keyboard and mouse events
class GRAPEENGINE_API Input {
public:
    // Initialize the input system with a GLFW window
    static void Initialize(GLFWwindow* pWin);

    // Check if a key is currently being pressed
    static bool IsKeyPressed(int key);

    // Check if a key is currently held down
    static bool IsKeyDown(int key);

    // Check if a key was just released this frame
    static bool IsKeyUp(int key);

    // Check if a mouse button is currently pressed
    static bool IsMousePressed(int button);

    /*!
    \brief Check if a mouse button is currently held down.
    \param button The mouse button to check (use MOUSE_* constants).
    \return True if the button is currently held down, false otherwise.

    This is equivalent to IsMousePressed(), but is intended for continuous
    checks such as dragging or camera movement.
    */
    static bool IsMouseDown(int button);

    static bool IsMouseReleased(const int button);

    /*!
    \brief Check if a mouse button was just released this frame.
    \param button The mouse button to check (use MOUSE_* constants).
    \return True if the button was just released, false otherwise.
    */
    static bool IsMouseUp(int button);

    /*!
    \brief Get the current mouse cursor position.
    \param xPos Reference to store the X coordinate.
    \param yPos Reference to store the Y coordinate.
    
    Retrieves the current mouse position in window coordinates.
    */
    static void GetMousePosition(double& xPos, double& yPos);

    // Get the current mouse X coordinate
    static double GetMouseX();

    // Get the current mouse Y coordinate
    static double GetMouseY();

    // Get the current horizontal scroll offset
    static double GetScrollX() { return m_scrollX; }

    // Get the current vertical scroll offset
    static double GetScrollY() { return m_scrollY; }

    // Set up all GLFW event callbacks for input handling
    static void SetupEventCallbacks();

    // GLFW error callback function
    static void ErrorCallback(int error, char const* description);

    // Get the character input buffer for this frame
    static const std::string& GetCharInput() { return m_charInput; }

    // Clear the character input buffer (call once per frame after processing)
    static void ClearCharInput() { m_charInput.clear(); }

    // Print OpenGL system specifications to the log
    static void PrintSpecs();

private:
    friend class Engine::Application;

    // GLFW window pointer for input handling
    static GLFWwindow* m_window;

    // ************** Keyboard state tracking ************** //
    static bool s_keyCurrent[MAX_KEYS];
    static bool s_keyPrevious[MAX_KEYS];

    // ************** Mouse state tracking ************** //
    static bool s_mouseCurrent[MAX_MOUSE];
    static bool s_mousePrevious[MAX_MOUSE];

    // Mouse wheel tracking
    static double m_scrollX;      // Horizontal scroll offset from last scroll event
    static double m_scrollY;      // Vertical scroll offset from last scroll event

    // Character input buffer
    static std::string m_charInput;   // Characters typed this frame

    // GLFW callback functions
    static void _keyCallback(GLFWwindow* pWin, int key, int scancode, int action, int mod);
    static void _mouseButtonCallback(GLFWwindow* pWin, int button, int action, int mod);
    static void _mousePosCallback(GLFWwindow* pWin, double xPos, double yPos);
    static void _mouseScrollCallback(GLFWwindow* pWin, double xOffset, double yOffset);
    static void _fileDropCallback(GLFWwindow* pWin, int count, const char** paths);
    static void _charCallback(GLFWwindow* pWin, unsigned int codepoint);

    // Process input events and update internal state (called once per frame)
    static void _processInput();
};


// Action constants
constexpr int PRESS     = GLFW_PRESS;
constexpr int REPEAT    = GLFW_REPEAT;
constexpr int RELEASE   = GLFW_RELEASE;

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
constexpr int KEY_F1    = GLFW_KEY_F1;
constexpr int KEY_F2    = GLFW_KEY_F2;
constexpr int KEY_F3    = GLFW_KEY_F3;
constexpr int KEY_F4    = GLFW_KEY_F4;
constexpr int KEY_F5    = GLFW_KEY_F5;
constexpr int KEY_F6    = GLFW_KEY_F6;
constexpr int KEY_F7    = GLFW_KEY_F7;
constexpr int KEY_F8    = GLFW_KEY_F8;
constexpr int KEY_F9    = GLFW_KEY_F9;
constexpr int KEY_F10   = GLFW_KEY_F10;
constexpr int KEY_F11   = GLFW_KEY_F11;
constexpr int KEY_F12   = GLFW_KEY_F12;
// Keyboard constants: special keys
constexpr int KEY_SPACE         = GLFW_KEY_SPACE;
constexpr int KEY_APOSTROPHE    = GLFW_KEY_APOSTROPHE;    // '
constexpr int KEY_COMMA         = GLFW_KEY_COMMA;         // ,
constexpr int KEY_MINUS         = GLFW_KEY_MINUS;         // -
constexpr int KEY_PERIOD        = GLFW_KEY_PERIOD;        // .
constexpr int KEY_SLASH         = GLFW_KEY_SLASH;         // /
constexpr int KEY_SEMICOLON     = GLFW_KEY_SEMICOLON;     // ;
constexpr int KEY_EQUAL         = GLFW_KEY_EQUAL;         // =
constexpr int KEY_LEFT_BRACKET  = GLFW_KEY_LEFT_BRACKET;  // [
constexpr int KEY_BACKSLASH     = GLFW_KEY_BACKSLASH;     // '\'
constexpr int KEY_RIGHT_BRACKET = GLFW_KEY_RIGHT_BRACKET; // ]
constexpr int KEY_GRAVE_ACCENT  = GLFW_KEY_GRAVE_ACCENT;  // `
constexpr int KEY_ESCAPE        = GLFW_KEY_ESCAPE;
constexpr int KEY_ENTER         = GLFW_KEY_ENTER;
constexpr int KEY_TAB           = GLFW_KEY_TAB;
constexpr int KEY_BACKSPACE     = GLFW_KEY_BACKSPACE;
constexpr int KEY_INSERT        = GLFW_KEY_INSERT;
constexpr int KEY_DELETE        = GLFW_KEY_DELETE;

// Keyboard constants: arrow keys
constexpr int KEY_RIGHT = GLFW_KEY_RIGHT;
constexpr int KEY_LEFT  = GLFW_KEY_LEFT;
constexpr int KEY_DOWN  = GLFW_KEY_DOWN;
constexpr int KEY_UP    = GLFW_KEY_UP;

// Keyboard constants: modifiers
constexpr int KEY_LEFT_SHIFT    = GLFW_KEY_LEFT_SHIFT;
constexpr int KEY_LEFT_CONTROL  = GLFW_KEY_LEFT_CONTROL;
constexpr int KEY_LEFT_ALT      = GLFW_KEY_LEFT_ALT;
constexpr int KEY_LEFT_SUPER    = GLFW_KEY_LEFT_SUPER;     // Windows/Command key
constexpr int KEY_RIGHT_SHIFT   = GLFW_KEY_RIGHT_SHIFT;
constexpr int KEY_RIGHT_CONTROL = GLFW_KEY_RIGHT_CONTROL;
constexpr int KEY_RIGHT_ALT     = GLFW_KEY_RIGHT_ALT;
constexpr int KEY_RIGHT_SUPER   = GLFW_KEY_RIGHT_SUPER;    // Windows/Command key

// Keyboard constants: navigation
constexpr int KEY_PAGE_UP   = GLFW_KEY_PAGE_UP;
constexpr int KEY_PAGE_DOWN = GLFW_KEY_PAGE_DOWN;
constexpr int KEY_HOME      = GLFW_KEY_HOME;
constexpr int KEY_END       = GLFW_KEY_END;
constexpr int KEY_CAPS_LOCK = GLFW_KEY_CAPS_LOCK;

// Mouse button constants
constexpr int MOUSE_LEFT    = GLFW_MOUSE_BUTTON_LEFT;
constexpr int MOUSE_RIGHT   = GLFW_MOUSE_BUTTON_RIGHT;
constexpr int MOUSE_MIDDLE  = GLFW_MOUSE_BUTTON_MIDDLE;

#endif
