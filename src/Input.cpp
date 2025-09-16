#include <Input.h>
#include <iostream>

// Initialize static member
GLFWwindow* Input::m_window = nullptr;

void Input::Init(GLFWwindow* pWin) { 
    m_window = pWin; 
}

// Check if a specific key is currently pressed
bool Input::IsKeyPressed(int key) {
    return glfwGetKey(m_window, key) == PRESS;
}

// Check if a specific mouse button is currently pressed  
bool Input::IsMousePressed(int button) {
    return glfwGetMouseButton(m_window, button) == PRESS;
}

// Get current mouse position
void Input::GetMousePos(double& xPos, double& yPos) {
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
    glfwSetFramebufferSizeCallback(m_window, _framebufferSizeCallback);
    glfwSetKeyCallback(m_window, _keyCallback);
    glfwSetMouseButtonCallback(m_window, _mouseButtonCallback);
    glfwSetCursorPosCallback(m_window, _mousePosCallback);
    glfwSetScrollCallback(m_window, _mouseScrollCallback);
}

// Called when GLFW encounters an error 
void Input::ErrorCallback(int error, char const* description) {
    (void)error;
#ifdef _DEBUG
    std::cerr << "GLFW error: " << description << std::endl;
#endif
}

// Called when window is resized
void Input::_framebufferSizeCallback(GLFWwindow* pWin, int width, int height) {
    (void)pWin;
    (void)width;
    (void)height;
#ifdef _DEBUG
    std::cout << "Window is being resized" << std::endl;
#endif
}

// Called on keyboard key press/release
void Input::_keyCallback(GLFWwindow* pWin, int key, int scancode, int action, int mod) {
    (void)pWin;
    (void)key;
    (void)scancode;
    (void)action;
    (void)mod;
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
    if (action == PRESS) std::cout << keyName << " key pressed\n";
    else if (action == REPEAT) std::cout << keyName << " key repeatedly pressed\n";
    else if (action == RELEASE) std::cout << keyName << " key released\n";
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
    if (action == PRESS) std::cout << buttonName << " pressed\n";
    else if (action == RELEASE) std::cout << buttonName << " released\n";
#endif
}

// Called when mouse cursor moves
void Input::_mousePosCallback(GLFWwindow* pWin, double xPos, double yPos) {
    (void)pWin;
    (void)xPos;
    (void)yPos;
#ifdef _DEBUG
    std::cout << "Mouse cursor position: (" << xPos << ", " << yPos << ")" << std::endl;
#endif
}

// Called when mouse wheel is scrolled
void Input::_mouseScrollCallback(GLFWwindow* pWin, double xOffset, double yOffset) {
    (void)pWin;
    (void)xOffset;
    (void)yOffset;
#ifdef _DEBUG
    std::cout << "Mouse scroll wheel offset: (" << xOffset << ", " << yOffset << ") " << std::endl;
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
    std::cout << "GPU Vendor: " << vendorStr << std::endl
        << "GL Renderer: " << rendererStr << std::endl
        << "GL Version: " << versionStr << std::endl
        << "GL Shader Version: " << shaderVersionStr << std::endl
        << "GL Major Version: " << majorVersion << std::endl
        << "GL Minor Version: " << minorVersion << std::endl
        << (doubleBuffer ? "Current OpenGL Context is double-buffered\n" : "Current OpenGL Context is not double-buffered\n")
        << "Maximum Vertex Count: " << maxVertices << std::endl
        << "Maximum Indices Count: " << maxIndices << std::endl
        << "GL Maximum texture size: " << maxTextureSize << std::endl
        << "Maximum Viewport Dimensions: " << maxViewportDims[0]
        << " x " << maxViewportDims[1] << std::endl
        << "Maximum generic vertex attributes: " << maxVertexAttribs << std::endl
        << "Maximum vertex buffer bindings: " << maxBufferBindings << std::endl << std::endl;
}
