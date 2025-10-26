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
};

/*!
\defgroup InputConstants Input Constants
\brief Predefined constants for input handling.

These constants provide convenient aliases for GLFW input codes,
making the code more readable and reducing dependency on GLFW headers.
@{
*/

//! \name Action Constants
//! @{
constexpr int PRESS     = GLFW_PRESS;   ///< Key/button press action
constexpr int REPEAT    = GLFW_REPEAT;  ///< Key repeat action
constexpr int RELEASE   = GLFW_RELEASE; ///< Key/button release action
//! @}

//! \name Keyboard Constants
//! @{
constexpr int KEY_W     = GLFW_KEY_W;     ///< W key
constexpr int KEY_A     = GLFW_KEY_A;     ///< A key
constexpr int KEY_S     = GLFW_KEY_S;     ///< S key
constexpr int KEY_D     = GLFW_KEY_D;     ///< D key
constexpr int KEY_C     = GLFW_KEY_C;     ///< C key
constexpr int KEY_P     = GLFW_KEY_P;     ///< P key
constexpr int KEY_SPACE = GLFW_KEY_SPACE; ///< Space key
constexpr int KEY_G     = GLFW_KEY_G;     ///< G key
constexpr int KEY_J     = GLFW_KEY_J;     ///< J key
constexpr int KEY_K     = GLFW_KEY_K;     ///< K key
constexpr int KEY_R     = GLFW_KEY_R;     ///< R key
constexpr int KEY_T     = GLFW_KEY_T;     ///< T key
//! @}

//! \name Mouse Button Constants
//! @{
constexpr int MOUSE_LEFT    = GLFW_MOUSE_BUTTON_LEFT;  ///< Left mouse button
constexpr int MOUSE_RIGHT   = GLFW_MOUSE_BUTTON_RIGHT; ///< Right mouse button
//! @}

/*! @} */ // end of InputConstants group

#endif
