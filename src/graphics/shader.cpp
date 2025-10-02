#include "../include/graphics/shader.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

// Helper: read file into string
std::string Shader::loadFile(const std::string& path) const {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Failed to open shader file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Helper: compile shader source
GLuint Shader::compileShader(GLenum type, const std::string& src) const {
    GLuint shader = glCreateShader(type);
    const char* code = src.c_str();
    glShaderSource(shader, 1, &code, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        std::string log(logLen, '\0');
        glGetShaderInfoLog(shader, logLen, nullptr, log.data());
        std::cerr << "Shader compilation failed:\n" << log << std::endl;
        glDeleteShader(shader);
        throw std::runtime_error("Shader compilation failed");
    }
    return shader;
}

// Constructor: build shader program from vertex + fragment files
Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath) {
    std::cout << "Current working directory: "
        << std::filesystem::current_path() << std::endl << std::endl;
    std::string vsrc = loadFile(vertexPath);
    std::string fsrc = loadFile(fragmentPath);

    GLuint vs = compileShader(GL_VERTEX_SHADER, vsrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsrc);

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);

    GLint linked = 0;
    glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint logLen = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &logLen);
        std::string log(logLen, '\0');
        glGetProgramInfoLog(m_program, logLen, nullptr, log.data());
        std::cerr << "Program link failed:\n" << log << std::endl;
        glDeleteProgram(m_program);
        m_program = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}

// Destructor: free program
Shader::~Shader() {
    if (m_program) {
        glDeleteProgram(m_program);
    }
}

// Move ctor
Shader::Shader(Shader&& other) noexcept {
    m_program = other.m_program;
    other.m_program = 0;
}

// Move assignment
Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (m_program) {
            glDeleteProgram(m_program);
        }
        m_program = other.m_program;
        other.m_program = 0;
    }
    return *this;
}

// Use program
void Shader::use() const {
    glUseProgram(m_program);
}

// Uniform setters
void Shader::setUniform(const std::string& name, bool value) const {
    glUniform1i(glGetUniformLocation(m_program, name.c_str()), (int)value);
}
void Shader::setUniform(const std::string& name, int value) const {
    glUniform1i(glGetUniformLocation(m_program, name.c_str()), value);
}
void Shader::setUniform(const std::string& name, float value) const {
    glUniform1f(glGetUniformLocation(m_program, name.c_str()), value);
}
void Shader::setUniform(const std::string& name, const glm::vec2& value) const {
    glUniform2f(glGetUniformLocation(m_program, name.c_str()), value.x, value.y);
}
void Shader::setUniform(const std::string& name, const glm::vec3& value) const {
    glUniform3f(glGetUniformLocation(m_program, name.c_str()), value.x, value.y, value.z);
}
void Shader::setUniform(const std::string& name, const glm::vec4& value) const {
    glUniform4f(glGetUniformLocation(m_program, name.c_str()), value.x, value.y, value.z, value.w);
}
void Shader::setUniform(const std::string& name, const glm::mat3& value) const {
    glUniformMatrix3fv(glGetUniformLocation(m_program, name.c_str()), 1, GL_FALSE, &value[0][0]);
}
void Shader::setUniform(const std::string& name, const glm::mat4& value) const {
    glUniformMatrix4fv(glGetUniformLocation(m_program, name.c_str()), 1, GL_FALSE, &value[0][0]);
}

void Shader::setMat4(const std::string& name, const glm::mat4& mat) const {
    GLint loc = glGetUniformLocation(m_program, name.c_str()); // ID = program handle
    if (loc == -1) {
        std::cerr << "Warning: uniform '" << name << "' not found in shader\n";
    }
    glUniformMatrix4fv(loc, 1, GL_FALSE, &mat[0][0]);
}