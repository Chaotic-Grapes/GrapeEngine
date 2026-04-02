/* Start Header *****************************************************************/
/*!
\file   Input.cpp
\author Foo Rui Qin (70%)
        Muhammad Nur Fadzly Bin Zulkifli (30%)
\par    ruiqin.foo@digipen.edu
        muhammadnurfadzly.b@digipen.edu
\date   12th March 2026
\brief
Implements the Input class for handling keyboard and mouse input events through
GLFW. Integrates with the engine's message system for event broadcasting and
logging system for error reporting.

Features:
- GLFW window initialization and event callback setup
- Real-time input state checking and frame-based state tracking
- Mouse position and scroll event handling
- Window resize event management
- OpenGL system specification reporting with detailed GPU information
- Message system integration for input event broadcasting
*/
/* End Header *******************************************************************/

#include "services/Input.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <sstream>
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include "core/Logger.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Xinput.h>
#endif

namespace {

#if defined(_WIN32)

using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
using XInputSetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_VIBRATION*);

struct XInputRuntime {
    HMODULE module = nullptr;
    XInputGetStateFn getState = nullptr;
    XInputSetStateFn setState = nullptr;
    bool initialized = false;
};

/**
 * @brief Resolve XInput symbols once and cache the result for process lifetime.
 * @return Reference to cached runtime dispatch table.
 * @note Dynamic loading avoids hard link dependencies on specific XInput DLL variants.
 * @complexity O(1) after first call, O(k) DLL probes on first call where k is candidate DLL count.
 */
XInputRuntime& GetXInputRuntime() {
    static XInputRuntime runtime{};
    if (runtime.initialized) {
        return runtime;
    }

    runtime.initialized = true;

    constexpr std::array<const char*, 4> kCandidates{
        "xinput1_4.dll",
        "xinput1_3.dll",
        "xinput9_1_0.dll",
        "xinput1_2.dll"
    };

    for (const char* dllName : kCandidates) {
        HMODULE module = ::LoadLibraryA(dllName);
        if (!module) {
            continue;
        }

        auto getState = reinterpret_cast<XInputGetStateFn>(::GetProcAddress(module, "XInputGetState"));
        auto setState = reinterpret_cast<XInputSetStateFn>(::GetProcAddress(module, "XInputSetState"));
        if (getState && setState) {
            runtime.module = module;
            runtime.getState = getState;
            runtime.setState = setState;
            break;
        }

        ::FreeLibrary(module);
    }

    return runtime;
}

/**
 * @brief Check whether XInput backend is available.
 * @return True when XInput function pointers were resolved.
 * @complexity O(1).
 */
bool IsXInputAvailable() {
    const XInputRuntime& runtime = GetXInputRuntime();
    return runtime.module != nullptr && runtime.getState != nullptr && runtime.setState != nullptr;
}

/**
 * @brief Check if a specific XInput user index currently has a connected controller.
 * @param userIndex XInput user index in [0, XUSER_MAX_COUNT).
 * @return True if controller state can be queried successfully.
 * @complexity O(1).
 */
bool IsXInputUserPresent(const DWORD userIndex) {
    if (!IsXInputAvailable() || userIndex >= XUSER_MAX_COUNT) {
        return false;
    }

    XINPUT_STATE state{};
    const XInputRuntime& runtime = GetXInputRuntime();
    return runtime.getState(userIndex, &state) == ERROR_SUCCESS;
}

/**
 * @brief Send vibration command to XInput controller.
 * @param userIndex XInput user index in [0, XUSER_MAX_COUNT).
 * @param lowFrequency Low-frequency motor intensity in [0, 1].
 * @param highFrequency High-frequency motor intensity in [0, 1].
 * @return True if command was accepted by XInput.
 * @complexity O(1).
 */
bool SetXInputVibration(const DWORD userIndex, const float lowFrequency, const float highFrequency) {
    if (!IsXInputAvailable() || userIndex >= XUSER_MAX_COUNT) {
        return false;
    }

    const float clampedLow = std::clamp(lowFrequency, 0.0f, 1.0f);
    const float clampedHigh = std::clamp(highFrequency, 0.0f, 1.0f);

    XINPUT_VIBRATION vibration{};
    vibration.wLeftMotorSpeed = static_cast<WORD>(clampedLow * 65535.0f + 0.5f);
    vibration.wRightMotorSpeed = static_cast<WORD>(clampedHigh * 65535.0f + 0.5f);

    const XInputRuntime& runtime = GetXInputRuntime();
    return runtime.setState(userIndex, &vibration) == ERROR_SUCCESS;
}

#endif

}

// Initialize static members
GLFWwindow* Input::m_window = nullptr;

// Keyboard state
bool Input::s_keyCurrent[MAX_KEYS] = { false };
bool Input::s_keyPrevious[MAX_KEYS] = { false };

// Mouse state
bool Input::s_mouseCurrent[MAX_MOUSE] = { false };
bool Input::s_mousePrevious[MAX_MOUSE] = { false };

// Gamepad state
bool Input::s_gamepadConnectedCurrent[MAX_GAMEPADS] = { false };
bool Input::s_gamepadConnectedPrevious[MAX_GAMEPADS] = { false };
bool Input::s_gamepadButtonCurrent[MAX_GAMEPADS][MAX_GAMEPAD_BUTTONS] = { false };
bool Input::s_gamepadButtonPrevious[MAX_GAMEPADS][MAX_GAMEPAD_BUTTONS] = { false };
float Input::s_gamepadAxisCurrent[MAX_GAMEPADS][MAX_GAMEPAD_AXES] = { 0.0f };

double Input::m_scrollX{ 0 };
double Input::m_scrollY{ 0 };
std::string Input::m_charInput{ "" };
std::vector<std::string> Input::m_droppedFiles{};

// Bind the GLFW window used by all static input queries and callback registration
void Input::Initialize(GLFWwindow* pWin) {
    m_window = pWin;
}

// Return true only on the frame the key transitions from up to down
bool Input::IsKeyPressed(const int key) {
    return s_keyCurrent[key] && !s_keyPrevious[key];
}

// Return true while the key is held down across frames
bool Input::IsKeyDown(const int key) {
    return s_keyCurrent[key];
}

// Return true only on the frame the key transitions from down to up
bool Input::IsKeyUp(const int key) {
    return !s_keyCurrent[key] && s_keyPrevious[key];
}

// Return true only on the frame the mouse button transitions from up to down
bool Input::IsMousePressed(const int button) {
    return s_mouseCurrent[button] && !s_mousePrevious[button];
}

// Return true while the mouse button is held down
bool Input::IsMouseDown(const int button) {
    return s_mouseCurrent[button];
}

// Return true only on the frame the mouse button transitions from down to up
bool Input::IsMouseUp(const int button) {
    return !s_mouseCurrent[button] && s_mousePrevious[button];
}

// Query current cursor coordinates directly from GLFW
void Input::GetMousePosition(double& xPos, double& yPos) {
    glfwGetCursorPos(m_window, &xPos, &yPos);
}

// Query only the X coordinate by reading both values and returning X
double Input::GetMouseX() {
    double xPos, yPos;
    glfwGetCursorPos(m_window, &xPos, &yPos);
    return xPos;
}

// Query only the Y coordinate by reading both values and returning Y
double Input::GetMouseY() {
    double xPos, yPos;
    glfwGetCursorPos(m_window, &xPos, &yPos);
    return yPos;
}

bool Input::IsGamepadConnected(const int gamepad) {
    if (!_isGamepadIndexValid(gamepad)) {
        return false;
    }
    return s_gamepadConnectedCurrent[gamepad];
}

bool Input::IsGamepadJustConnected(const int gamepad) {
    if (!_isGamepadIndexValid(gamepad)) {
        return false;
    }
    return s_gamepadConnectedCurrent[gamepad] && !s_gamepadConnectedPrevious[gamepad];
}

bool Input::IsGamepadJustDisconnected(const int gamepad) {
    if (!_isGamepadIndexValid(gamepad)) {
        return false;
    }
    return !s_gamepadConnectedCurrent[gamepad] && s_gamepadConnectedPrevious[gamepad];
}

bool Input::IsGamepadButtonPressed(const int gamepad, const int button) {
    if (!_isGamepadIndexValid(gamepad) || !_isGamepadButtonValid(button) || !s_gamepadConnectedCurrent[gamepad]) {
        return false;
    }
    return s_gamepadButtonCurrent[gamepad][button] && !s_gamepadButtonPrevious[gamepad][button];
}

bool Input::IsGamepadButtonDown(const int gamepad, const int button) {
    if (!_isGamepadIndexValid(gamepad) || !_isGamepadButtonValid(button) || !s_gamepadConnectedCurrent[gamepad]) {
        return false;
    }
    return s_gamepadButtonCurrent[gamepad][button];
}

bool Input::IsGamepadButtonUp(const int gamepad, const int button) {
    if (!_isGamepadIndexValid(gamepad) || !_isGamepadButtonValid(button)) {
        return false;
    }
    return !s_gamepadButtonCurrent[gamepad][button] && s_gamepadButtonPrevious[gamepad][button];
}

float Input::GetGamepadAxis(const int gamepad, const int axis) {
    if (!_isGamepadIndexValid(gamepad) || !_isGamepadAxisValid(axis) || !s_gamepadConnectedCurrent[gamepad]) {
        return 0.0f;
    }
    return s_gamepadAxisCurrent[gamepad][axis];
}

float Input::GetGamepadAxisWithDeadzone(const int gamepad, const int axis, const float deadzone) {
    return _applyAxisDeadzone(GetGamepadAxis(gamepad, axis), deadzone);
}

/**
 * @brief Set controller vibration for a gamepad slot.
 * @param gamepad Gamepad slot index in [0, MAX_GAMEPADS).
 * @param lowFrequency Low-frequency motor intensity in [0, 1].
 * @param highFrequency High-frequency motor intensity in [0, 1].
 * @return True when vibration was sent successfully.
 * @note Current implementation uses XInput on Windows and supports slots 0..3.
 * @complexity O(1).
 */
bool Input::SetGamepadVibration(const int gamepad, const float lowFrequency, const float highFrequency) {
    if (!_isGamepadIndexValid(gamepad) || !IsGamepadConnected(gamepad)) {
        return false;
    }

#if defined(_WIN32)
    if (gamepad < 0 || gamepad >= static_cast<int>(XUSER_MAX_COUNT)) {
        return false;
    }

    const DWORD userIndex = static_cast<DWORD>(gamepad);
    if (!IsXInputUserPresent(userIndex)) {
        return false;
    }

    return SetXInputVibration(userIndex, lowFrequency, highFrequency);
#else
    (void)lowFrequency;
    (void)highFrequency;
    return false;
#endif
}

/**
 * @brief Stop controller vibration for a gamepad slot.
 * @param gamepad Gamepad slot index in [0, MAX_GAMEPADS).
 * @return void
 * @complexity O(1).
 */
void Input::StopGamepadVibration(const int gamepad) {
    (void)SetGamepadVibration(gamepad, 0.0f, 0.0f);
}

/**
 * @brief Check whether vibration is supported for a gamepad slot.
 * @param gamepad Gamepad slot index in [0, MAX_GAMEPADS).
 * @return True if vibration can be sent for the given slot.
 * @complexity O(1).
 */
bool Input::IsGamepadVibrationSupported(const int gamepad) {
    if (!_isGamepadIndexValid(gamepad) || !IsGamepadConnected(gamepad)) {
        return false;
    }

#if defined(_WIN32)
    if (gamepad < 0 || gamepad >= static_cast<int>(XUSER_MAX_COUNT)) {
        return false;
    }

    return IsXInputUserPresent(static_cast<DWORD>(gamepad));
#else
    return false;
#endif
}

// Register all GLFW callbacks so input state and message broadcasts stay synchronized
void Input::SetupEventCallbacks() {
    glfwSetKeyCallback(m_window, _keyCallback);
    glfwSetMouseButtonCallback(m_window, _mouseButtonCallback);
    glfwSetCursorPosCallback(m_window, _mousePosCallback);
    glfwSetScrollCallback(m_window, _mouseScrollCallback);
    glfwSetDropCallback(m_window, _fileDropCallback);
    glfwSetCharCallback(m_window, _charCallback);
}

// Move out dropped files and clear the internal queue in one operation
std::vector<std::string> Input::ConsumeDroppedFiles() {
    std::vector<std::string> dropped;
    // Swap avoids copying strings and leaves the member vector empty for future callbacks
    dropped.swap(m_droppedFiles);
    return dropped;
}

// Forward GLFW error information into the engine logger
void Input::ErrorCallback(const int error, char const* description) {
    (void)error;
    LOG_ERROR("GLFW error: " << description);
}

// Advance per-frame input history, clear one-frame deltas, then pump GLFW events
void Input::_processInput() {
    // Snapshot current button states so edge detection still works next frame
    std::memcpy(s_keyPrevious,   s_keyCurrent,   sizeof(s_keyCurrent));
    std::memcpy(s_mousePrevious, s_mouseCurrent, sizeof(s_mouseCurrent));

    // Scroll is treated as a per-frame delta, so clear before processing new events
    m_scrollX = 0.0;
    m_scrollY = 0.0;

    // Text input is also frame-scoped and rebuilt by _charCallback events
    m_charInput.clear();

    // Dispatch pending OS events, which will invoke our registered callbacks
    glfwPollEvents();

    // Poll gamepads after event pump so connection and state reflect this frame
    _updateGamepadStates();
}

void Input::_updateGamepadStates() {
    // Preserve previous-frame gamepad state for edge detection
    std::memcpy(s_gamepadConnectedPrevious, s_gamepadConnectedCurrent, sizeof(s_gamepadConnectedCurrent));
    std::memcpy(s_gamepadButtonPrevious, s_gamepadButtonCurrent, sizeof(s_gamepadButtonCurrent));

    for (int gamepad = 0; gamepad < MAX_GAMEPADS; ++gamepad) {
        const int joystickId = GLFW_JOYSTICK_1 + gamepad;
        const bool present = glfwJoystickPresent(joystickId) == GLFW_TRUE;
        const bool isMappedGamepad = present && (glfwJoystickIsGamepad(joystickId) == GLFW_TRUE);

        s_gamepadConnectedCurrent[gamepad] = isMappedGamepad;

        if (!isMappedGamepad) {
            // Ensure stale state is cleared immediately on disconnect or unsupported devices
            std::memset(s_gamepadButtonCurrent[gamepad], 0, sizeof(s_gamepadButtonCurrent[gamepad]));
            std::fill(std::begin(s_gamepadAxisCurrent[gamepad]), std::end(s_gamepadAxisCurrent[gamepad]), 0.0f);
            continue;
        }

        GLFWgamepadstate state{};
        if (glfwGetGamepadState(joystickId, &state) != GLFW_TRUE) {
            // Treat transient read failures as disconnected for stable behavior
            s_gamepadConnectedCurrent[gamepad] = false;
            std::memset(s_gamepadButtonCurrent[gamepad], 0, sizeof(s_gamepadButtonCurrent[gamepad]));
            std::fill(std::begin(s_gamepadAxisCurrent[gamepad]), std::end(s_gamepadAxisCurrent[gamepad]), 0.0f);
            continue;
        }

        for (int button = 0; button < MAX_GAMEPAD_BUTTONS; ++button) {
            s_gamepadButtonCurrent[gamepad][button] = (state.buttons[button] == GLFW_PRESS);
        }
        for (int axis = 0; axis < MAX_GAMEPAD_AXES; ++axis) {
            s_gamepadAxisCurrent[gamepad][axis] = state.axes[axis];
        }
    }
}

bool Input::_isGamepadIndexValid(const int gamepad) {
    return gamepad >= 0 && gamepad < MAX_GAMEPADS;
}

bool Input::_isGamepadButtonValid(const int button) {
    return button >= 0 && button < MAX_GAMEPAD_BUTTONS;
}

bool Input::_isGamepadAxisValid(const int axis) {
    return axis >= 0 && axis < MAX_GAMEPAD_AXES;
}

float Input::_applyAxisDeadzone(const float value, float deadzone) {
    deadzone = std::clamp(deadzone, 0.0f, 0.999f);

    const float absValue = std::fabs(value);
    if (absValue <= deadzone) {
        return 0.0f;
    }

    const float scaled = (absValue - deadzone) / (1.0f - deadzone);
    return (value < 0.0f ? -scaled : scaled);
}

// Update keyboard state and broadcast key transition events
void Input::_keyCallback(GLFWwindow*, int key, int, int action, int mod) {
    // Ignore unknown keys and out-of-range values returned by platform backends
    if (key < 0 || key >= MAX_KEYS)
        return;

    if (action == GLFW_PRESS) {
        s_keyCurrent[key] = true;
        Messaging::MessageSystem::Broadcast(Messaging::KeyPressed{ key, false, mod });
    }
    else if (action == GLFW_RELEASE) {
        s_keyCurrent[key] = false;
        Messaging::MessageSystem::Broadcast(Messaging::KeyReleased{ key, mod });
    }
}

// Update mouse button state and broadcast click/release with cursor position payload
void Input::_mouseButtonCallback(GLFWwindow*, int button, int action, int) {
    // Ignore out-of-range button indices so arrays remain safe
    if (button < 0 || button >= MAX_MOUSE)
        return;

    if (action == GLFW_PRESS) {
        double xPos = 0.0, yPos = 0.0;
        if (Input::m_window) {
            // Read cursor position at click time so listeners get precise interaction coordinates
            glfwGetCursorPos(Input::m_window, &xPos, &yPos);
        }
        s_mouseCurrent[button] = true;
        Messaging::MessageSystem::Broadcast(Messaging::MouseButtonPressed{ button,
            static_cast<float>(xPos), static_cast<float>(yPos) });
    }
    else if (action == GLFW_RELEASE) {
        double xPos = 0.0, yPos = 0.0;
        if (Input::m_window) {
            // Read release position to support drag-end and drop style interactions
            glfwGetCursorPos(Input::m_window, &xPos, &yPos);
        }
        s_mouseCurrent[button] = false;
        Messaging::MessageSystem::Broadcast(Messaging::MouseButtonReleased{ button,
            static_cast<float>(xPos), static_cast<float>(yPos) });
    }
}

// Enhanced mouse position callback with delta tracking
static double lastMouseX = 0.0, lastMouseY = 0.0;

// Broadcast absolute cursor position plus per-event delta for camera and gizmo controls
void Input::_mousePosCallback(GLFWwindow* pWin, double xPos, double yPos) {
    (void)pWin;
    
    // Delta is measured against the previously observed cursor sample
    double deltaX = xPos - lastMouseX;
    double deltaY = yPos - lastMouseY;

    // Broadcast both absolute and delta coordinates so subscribers choose what they need
    Messaging::MessageSystem::Broadcast(Messaging::MouseMoved{ static_cast<float>(xPos),
        static_cast<float>(yPos), static_cast<float>(deltaX), static_cast<float>(deltaY) });

    // Persist the sample for next callback delta computation
    lastMouseX = xPos;
    lastMouseY = yPos;
}

// Store frame scroll deltas and broadcast a scroll event
void Input::_mouseScrollCallback(GLFWwindow* pWin, double xOffset, double yOffset) {
    (void)pWin;

    // Store latest deltas so polling APIs can consume scroll this frame
    m_scrollX = xOffset;
    m_scrollY = yOffset;

    // Publish one-shot wheel movement for systems that are event-driven
    Messaging::MessageSystem::Broadcast(Messaging::MouseScrolled{ static_cast<float>(xOffset),
        static_cast<float>(yOffset) }); // One-shot deltas; cleared at start of each frame
}

// Queue dropped file paths and broadcast each one so tools can react immediately
void Input::_fileDropCallback(GLFWwindow* pWin, int count, const char** paths) {
    (void)pWin;

    if (count > 0) {
        // Iterate all dropped items because OS drag-drop can deliver multiple files at once
        for (int i = 0; i < count; i++) {
            // Copy the C string path into owned storage before callback stack memory is released
            std::string droppedPath(paths[i]);
            m_droppedFiles.push_back(droppedPath);
            Messaging::MessageSystem::Broadcast(Messaging::FileDropped{ droppedPath });
        }
    }
}

// Convert incoming UTF-32 codepoints into UTF-8 bytes appended to this frame's text buffer
void Input::_charCallback(GLFWwindow* pWin, unsigned int codepoint) {
    (void)pWin;

    // Encode single-byte ASCII codepoints as-is
    if (codepoint < 0x80) {
        m_charInput += static_cast<char>(codepoint);
    }
    else if (codepoint < 0x800) {
        // Encode two-byte UTF-8 sequences: 110xxxxx 10xxxxxx
        m_charInput += static_cast<char>(0xC0 | (codepoint >> 6));
        m_charInput += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
    else if (codepoint < 0x10000) {
        // Encode three-byte UTF-8 sequences for BMP codepoints
        m_charInput += static_cast<char>(0xE0 | (codepoint >> 12));
        m_charInput += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        m_charInput += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
    else if (codepoint < 0x110000) {
        // Encode four-byte UTF-8 sequences for supplementary planes
        m_charInput += static_cast<char>(0xF0 | (codepoint >> 18));
        m_charInput += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        m_charInput += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        m_charInput += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
}

// Query OpenGL driver capabilities and print a consolidated hardware report
void Input::PrintSpecs() {
    // String queries provide driver identity and language support details
    const GLubyte* vendorStr = glGetString(GL_VENDOR);       // GPU vendor
    const GLubyte* rendererStr = glGetString(GL_RENDERER);   // Renderer/device string
    const GLubyte* versionStr = glGetString(GL_VERSION);     // OpenGL runtime version
    const GLubyte* shaderVersionStr = glGetString(GL_SHADING_LANGUAGE_VERSION);  // GLSL version

    // Numeric queries provide hard limits used for rendering budget decisions
    GLint majorVersion, minorVersion;
    glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);          // API major version
    glGetIntegerv(GL_MINOR_VERSION, &minorVersion);          // API minor version

    // Double buffering state indicates whether front/back swap presentation is enabled
    GLint doubleBuffer;
    glGetIntegerv(GL_DOUBLEBUFFER, &doubleBuffer);

    // Capability limits determine practical mesh, viewport, and attribute constraints
    GLint maxVertices, maxIndices, maxTextureSize, maxVertexAttribs, maxBufferBindings;
    GLint maxViewportDims[2];
    glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &maxVertices);   // Recommended indexed draw vertex bound
    glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &maxIndices);     // Recommended indexed draw index bound
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);     // Largest square texture dimension
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, maxViewportDims);    // Maximum viewport width and height
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs); // Maximum vertex input attributes
    glGetIntegerv(GL_MAX_VERTEX_ATTRIB_BINDINGS, &maxBufferBindings); // Maximum vertex buffer binding slots

    // Emit a single structured log block for easier copy and diagnostics
    LOG_INFO("=== GPU Specifications ===" << "\n"
        << "GPU Vendor: " << vendorStr << "\n"
        << "GL Renderer: " << rendererStr << "\n"
        << "GL Version: " << versionStr << "\n"
        << "GL Shader Version: " << shaderVersionStr << "\n"
        << "GL Major Version: " << majorVersion << "\n"
        << "GL Minor Version: " << minorVersion << "\n"
        << (doubleBuffer ? "Current OpenGL Context is double-buffered\n" : "Current OpenGL Context is not double-buffered\n")
        << "Maximum Vertex Count: " << maxVertices << "\n"
        << "Maximum Indices Count: " << maxIndices << "\n"
        << "GL Maximum texture size: " << maxTextureSize << "\n"
        << "Maximum Viewport Dimensions: " << maxViewportDims[0] << " x " << maxViewportDims[1] << "\n"
        << "Maximum generic vertex attributes: " << maxVertexAttribs << "\n"
        << "Maximum vertex buffer bindings: " << maxBufferBindings);
}
