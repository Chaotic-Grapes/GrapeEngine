#include <Input.h>
#include <iostream>
#include <iomanip>
#include <sstream>

// Initialize static members
GLFWwindow* Input::m_window = nullptr;
std::unordered_map<int, bool> Input::m_keyDown{ 0 };
std::unordered_map<int, bool> Input::m_keyPressed{ 0 };
std::unordered_map<int, bool> Input::m_keyUp{ 0 };
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
bool Input::IsKeyPressed(const int key) { return m_keyPressed[key]; }

// Check if a specific key was just pressed this frame
bool Input::IsKeyDown(const int key) { return m_keyDown[key]; }

// Check if a specific key was just released this frame
bool Input::IsKeyUp(const int key) { return m_keyUp[key]; }

// Check if a specific mouse button is currently pressed  
bool Input::IsMousePressed(const int button) {
    return glfwGetMouseButton(m_window, button) == PRESS;
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
}

// Called when GLFW encounters an error 
void Input::ErrorCallback(const int error, char const* description) {
    (void)error;
#ifdef _DEBUG
    std::cerr << "GLFW error: " << description << "\n";
#endif
}

// Called when window is resized
void Input::_windowSizeCallback(GLFWwindow* pWin, int width, int height) {
    (void)pWin;

    // Store the new window dimensions
    m_windowWidth = width;
    m_windowHeight = height;

#ifdef _DEBUG
    std::ostringstream oss;
    oss << "Window is being resized: " << width << "x" << height;
    std::cout << "\r" << std::setw(50) << std::left << oss.str() << std::flush;
#endif
}

void Input::_processInput() {
    glfwPollEvents();

    m_keyPressed.clear();
    m_keyDown.clear();
}


// Called on keyboard key press/release
void Input::_keyCallback(GLFWwindow* pWin, int key, int scancode, int action, int mod) {
    (void)pWin;
    (void)scancode;
    (void)mod;

    if (action == GLFW_PRESS) {
        m_keyDown[key] = true;
        m_keyPressed[key] = true;
    }
    else if (action == GLFW_RELEASE) {
        m_keyDown[key] = false;
        m_keyUp[key] = true;
    }

#ifdef _DEBUG
    const char* keyName = "";
    switch (key) {
    case KEY_W: keyName = "W"; break;
    case KEY_A: keyName = "A"; break;
    case KEY_S: keyName = "S"; break;
    case KEY_D: keyName = "D"; break;
    default: keyName = "Unknown"; break;
    }
    // Debug-only code
    std::string message;
    if (action == PRESS)
        message = "[Key] " + std::string(keyName) + " key pressed";
    else if (action == REPEAT)
        message = "[Key] " + std::string(keyName) + " key repeatedly pressed";
    else if (action == RELEASE)
        message = "[Key] " + std::string(keyName) + " key released";

    std::cout << "\r" << std::setw(50) << std::left << message << std::flush;
#endif
}

// Called on mouse button press/release
void Input::_mouseButtonCallback(GLFWwindow* pWin, int button, int action, int mod) {
    (void)pWin;
    (void)button;
    (void)action;
    (void)mod;
#ifdef _DEBUG
    const char* buttonName = "";
    switch (button) {
    case MOUSE_LEFT: buttonName = "Left mouse button"; break;
    case MOUSE_RIGHT: buttonName = "Right mouse button"; break;
    default: buttonName = "Mouse button"; break;
    }
    // Debug-only code
    std::string message;
    if (action == PRESS)
        message = "[Mouse] " + std::string(buttonName) + " pressed";
    else if (action == RELEASE)
        message = "[Mouse] " + std::string(buttonName) + " released";

    std::cout << "\r" << std::setw(50) << std::left << message << std::flush;
#endif
}

// Called when mouse cursor moves
void Input::_mousePosCallback(GLFWwindow* pWin, double xPos, double yPos) {
    (void)pWin;
    (void)xPos;
    (void)yPos;
#ifdef _DEBUG
    std::ostringstream oss;
    oss << "Mouse cursor position: (" << std::fixed << std::setprecision(1) << xPos << ", " << yPos << ")";
    std::cout << "\r" << std::setw(50) << std::left << oss.str() << std::flush;
#endif
}

// Called when mouse wheel is scrolled
void Input::_mouseScrollCallback(GLFWwindow* pWin, double xOffset, double yOffset) {
    (void)pWin;
    (void)xOffset;
    (void)yOffset;

    // Store the scroll offsets
    m_scrollX = xOffset;
    m_scrollY = yOffset;

#ifdef _DEBUG
    std::ostringstream oss;
    oss << "Mouse scroll wheel offset: (" << std::fixed << std::setprecision(1) << xOffset << ", " << yOffset << ")";
    std::cout << "\r" << std::setw(50) << std::left << oss.str() << std::flush;
#endif
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
    std::cout << "GPU Vendor: " << vendorStr << "\n"
        << "GL Renderer: " << rendererStr << "\n"
        << "GL Version: " << versionStr << "\n"
        << "GL Shader Version: " << shaderVersionStr << "\n"
        << "GL Major Version: " << majorVersion << "\n"
        << "GL Minor Version: " << minorVersion << "\n"
        << (doubleBuffer ? "Current OpenGL Context is double-buffered\n" : "Current OpenGL Context is not double-buffered\n")
        << "Maximum Vertex Count: " << maxVertices << "\n"
        << "Maximum Indices Count: " << maxIndices << "\n"
        << "GL Maximum texture size: " << maxTextureSize << "\n"
        << "Maximum Viewport Dimensions: " << maxViewportDims[0]
        << " x " << maxViewportDims[1] << "\n"
        << "Maximum generic vertex attributes: " << maxVertexAttribs << "\n"
        << "Maximum vertex buffer bindings: " << maxBufferBindings << "\n\n";
}
