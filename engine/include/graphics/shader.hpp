/* Start Header *****************************************************************/
/*!
\file   shader.hpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   3rd October 2025
\brief
The Shader class is a lightweight RAII wrapper for an OpenGL shader program.
It compiles and links vertex/fragment shaders from source files, manages the
underlying OpenGL program object, and provides helper functions to set uniform
variables of various types (bool, int, float, vectors, and matrices).

*/
/* End Header *******************************************************************/

#pragma once
#include "Export.h"
#include <string>
#include <glm/glm.hpp>
#include <glad/glad.h>

class GRAPEENGINE_API Shader {
public:
    Shader() = default;
    // Load, compile, and link a shader program from vertex/fragment source files.
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    // Release the owned OpenGL program.
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    // Bind this shader program as the current OpenGL program.
    void use() const;

    // Uniform setters
    void setUniform(const std::string& name, bool value) const;
    void setUniform(const std::string& name, int value) const;
    void setUniform(const std::string& name, float value) const;
    void setUniform(const std::string& name, const glm::vec2& value) const;
    void setUniform(const std::string& name, const glm::vec3& value) const;
    void setUniform(const std::string& name, const glm::vec4& value) const;
    void setUniform(const std::string& name, const glm::mat3& value) const;
    void setUniform(const std::string& name, const glm::mat4& value) const;

    void setMat4(const std::string& name, const glm::mat4& mat) const;

    GLuint id() const { return m_program; }

private:
    GLuint m_program = 0;

    // Read a shader source file into memory.
    std::string loadFile(const std::string& path) const;
    // Compile one shader stage and return its OpenGL handle.
    GLuint compileShader(GLenum type, const std::string& src) const;
};