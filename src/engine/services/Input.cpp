/* Start Header *****************************************************************/
/*!
\file   Input.cpp
\author Foo Rui Qin (70%)
        Muhammad Nur Fadzly Bin Zulkifli (30%)
\par    ruiqin.foo@digipen.edu
        muhammadnurfadzly.b@digipen.edu
\date   26th October 2025
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
#include <sstream>
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include "core/Logger.h"

// Initialize static members
GLFWwindow* Input::m_window = nullptr;
std::unordered_map<int, bool> Input::m_keyDown{ 0 };
std::unordered_map<int, bool> Input::m_keyPressed{ 0 };
std::unordered_map<int, bool> Input::m_keyUp{ 0 };

std::unordered_map<int, bool> Input::m_mouseDown{ 0 };
std::unordered_map<int, bool> Input::m_mousePressed{ 0 };
std::unordered_map<int, bool> Input::m_mouseUp{ 0 };

int Input::m_windowWidth{ 0 };
int Input::m_windowHeight{ 0 };
double Input::m_scrollX{ 0 };
double Input::m_scrollY{ 0 };

void Input::Initialize(GLFWwindow* pWin) {
    m_window = pWin;
    // Get initial window size
    glfwGetWindowSize(m_window, &m_windowWidth, &m_windowHeight);
}

// Check if a specific key is currently pressed
bool Input::IsKeyPressed(const int key) {
    return m_keyPressed[key];
}

// Check if a specific key was just pressed this frame
bool Input::IsKeyDown(const int key) {
    return glfwGetKey(m_window, key) == PRESS;
}

// Check if a specific key was just released this frame
bool Input::IsKeyUp(const int key) {
    return m_keyUp[key];
}

// Check if a specific mouse button is currently pressed
bool Input::IsMousePressed(const int button) {
    return glfwGetMouseButton(m_window, button) == PRESS;
}

// Check if a mouse button was just pressed this frame
bool Input::IsMouseDown(const int button) {
    return glfwGetMouseButton(m_window, button) == GLFW_PRESS;
}

// Check if a mouse button was just released this frame
bool Input::IsMouseUp(const int button) {
    return m_mouseUp[button];
}

// Get current mouse position
void Input::GetMousePosition(double& xPos, double& yPos) {
    glfwGetCursorPos(m_window, &xPos, &yPos);
}

// Get mouse X position
double Input::GetMouseX() {
    double xPos, yPos;
    glfwGetCursorPos(m_window, &xPos, &yPos);
    return xPos;
}

// Get mouse Y position
double Input::GetMouseY() {
    double xPos, yPos;
    glfwGetCursorPos(m_window, &xPos, &yPos);
    return yPos;
}

// Sets up all GLFW event callbacks (keyboard, mouse, resize)
void Input::SetupEventCallbacks() {
    glfwSetKeyCallback(m_window, _keyCallback);
    glfwSetMouseButtonCallback(m_window, _mouseButtonCallback);
    glfwSetCursorPosCallback(m_window, _mousePosCallback);
    glfwSetScrollCallback(m_window, _mouseScrollCallback);
    glfwSetWindowSizeCallback(m_window, _windowSizeCallback);
    glfwSetWindowSizeCallback(m_window, _fileDropCallback);
}

// Called when GLFW encounters an error
void Input::ErrorCallback(const int error, char const* description) {
    (void)error;
    LOG_ERROR("GLFW error: " << description);
}

// Called when window is resized
void Input::_windowSizeCallback(GLFWwindow* pWin, int width, int height) {
    (void)pWin;

    // Store the new window dimensions
    m_windowWidth = width;
    m_windowHeight = height;
}

// Clear frame-specific input state and poll GLFW events (called once per frame)
void Input::_processInput() {
    m_keyDown.clear();
    m_mouseDown.clear();
    m_mousePressed.clear();
    m_mouseUp.clear();
    m_keyUp.clear();
    m_keyPressed.clear();
    m_mouseDown.clear();
    m_mouseUp.clear();

    // Reset scroll deltas so scroll input only lasts one frame
    m_scrollX = 0.0;
    m_scrollY = 0.0;

    glfwPollEvents();
}

// Called on keyboard key press/release
void Input::_keyCallback(GLFWwindow* pWin, int key, int scancode, int action, int mod) {
    (void)pWin;
    (void)mod;
    (void)scancode;

    if (action == GLFW_PRESS) {
        m_keyDown[key] = true;
        m_keyPressed[key] = true;
        Messaging::MessageSystem::Broadcast(Messaging::KeyPressed{ key, false, mod });
    }
    else if (action == GLFW_RELEASE) {
        m_keyDown[key] = false;
        m_keyUp[key] = true;
        Messaging::MessageSystem::Broadcast(Messaging::KeyReleased{ key, mod });
    }
}

// Called on mouse button press/release
void Input::_mouseButtonCallback(GLFWwindow* pWin, int button, int action, int mod) {
    (void)pWin;
    (void)mod;

    if (action == GLFW_PRESS) {
        m_mouseDown[button] = true;
        m_mousePressed[button] = true;
        Messaging::MessageSystem::Broadcast(Messaging::MousePressed{ button,
            static_cast<float>(xPos), static_cast<float>(yPos) });
    }
    else if (action == GLFW_RELEASE) {
        m_mouseDown[button] = false;
        m_mouseUp[button] = true;
        Messaging::MessageSystem::Broadcast(Messaging::MouseReleased{ button,
            static_cast<float>(xPos), static_cast<float>(yPos) });
    }
}

// Enhanced mouse position callback with delta tracking
static double lastMouseX = 0.0, lastMouseY = 0.0;

// Called when mouse cursor moves
void Input::_mousePosCallback(GLFWwindow* pWin, double xPos, double yPos) {
    (void)pWin;
    
    // Calculate delta from last position
    double deltaX = xPos - lastMouseX;
    double deltaY = yPos - lastMouseY;

    // Broadcast mouse movement event
    Messaging::MessageSystem::Broadcast(Messaging::MouseMoved{ static_cast<float>(xPos),
        static_cast<float>(yPos), static_cast<float>(deltaX), static_cast<float>(deltaY) });

    // Update last position
    lastMouseX = xPos;
    lastMouseY = yPos;
}

// Called when mouse wheel is scrolled
void Input::_mouseScrollCallback(GLFWwindow* pWin, double xOffset, double yOffset) {
    (void)pWin;

    // Store the scroll offsets
    m_scrollX = xOffset;
    m_scrollY = yOffset;

    // Broadcast scroll event
    Messaging::MessageSystem::Broadcast(Messaging::MouseScrolled{ static_cast<float>(xOffset),
        static_cast<float>(yOffset) }); // One-shot deltas; cleared at start of each frame
}

void Input::_fileDropCallback(GLFWwindow* pWin, int count, const char** paths) {
    (void)pWin;

    if (count > 0) {
        // Broadcast event for each dropped file
        for (int i = 0; i < count; ++i) {
            Messaging::MessageSystem::Broadcast(Messaging::FileDropped{ std::string(paths[i]) });
        }
    }
}

// Prints OpenGL system info (GPU, version, limits, etc.)
void Input::PrintSpecs() {
    // glGetString() for OpenGL string information
    const GLubyte* vendorStr = glGetString(GL_VENDOR);       // Vendor name
    const GLubyte* rendererStr = glGetString(GL_RENDERER);   // Renderer name
    const GLubyte* versionStr = glGetString(GL_VERSION);     // Graphics driver version
    const GLubyte* shaderVersionStr = glGetString(GL_SHADING_LANGUAGE_VERSION);  // Shader language version

    // glGetIntegerv() for OpenGL numeric parameters
    GLint majorVersion, minorVersion;
    glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);          // Major version
    glGetIntegerv(GL_MINOR_VERSION, &minorVersion);          // Minor version

    // Buffer and context information
    GLint doubleBuffer;
    glGetIntegerv(GL_DOUBLEBUFFER, &doubleBuffer);           // Double buffering status

    // System limits
    GLint maxVertices, maxIndices, maxTextureSize, maxVertexAttribs, maxBufferBindings;
    GLint maxViewportDims[2];
    glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &maxVertices);   // Max vertex count
    glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &maxIndices);     // Max index count
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);     // Max texture size
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, maxViewportDims);    // Max viewport dimensions
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs); // Max vertex attributes
    glGetIntegerv(GL_MAX_VERTEX_ATTRIB_BINDINGS, &maxBufferBindings); // Max buffer bindings

    // Print to output
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
