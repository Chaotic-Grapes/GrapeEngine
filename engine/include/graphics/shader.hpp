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
    /**
     * @brief Load, compile, and link a shader program from vertex/fragment source files.
     * @param vertexPath File path to the GLSL vertex shader source.
     * @param fragmentPath File path to the GLSL fragment shader source.
     */
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    // Release the owned OpenGL program.
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    /**
     * @brief Move constructor; transfers program ownership without recompiling.
     * @param other Source shader to move from (left in a null state).
     */
    Shader(Shader&& other) noexcept;

    /**
     * @brief Move assignment; transfers program ownership without recompiling.
     * @param other Source shader to move from (left in a null state).
     * @return Reference to this shader after assignment.
     */
    Shader& operator=(Shader&& other) noexcept;

    /**
     * @brief Bind this shader program as the current OpenGL program.
     */
    void use() const;

    /**
     * @brief Set a named uniform variable; silently ignored if the name is not active.
     * @param name Uniform variable name in the shader source.
     * @param value Value to upload.
     */
    void setUniform(const std::string& name, bool value) const;

    /**
     * @brief Set a named integer uniform variable.
     * @param name Uniform variable name in the shader source.
     * @param value Integer value to upload.
     */
    void setUniform(const std::string& name, int value) const;

    /**
     * @brief Set a named float uniform variable.
     * @param name Uniform variable name in the shader source.
     * @param value Float value to upload.
     */
    void setUniform(const std::string& name, float value) const;

    /**
     * @brief Set a named vec2 uniform variable.
     * @param name Uniform variable name in the shader source.
     * @param value 2-component vector value to upload.
     */
    void setUniform(const std::string& name, const glm::vec2& value) const;

    /**
     * @brief Set a named vec3 uniform variable.
     * @param name Uniform variable name in the shader source.
     * @param value 3-component vector value to upload.
     */
    void setUniform(const std::string& name, const glm::vec3& value) const;

    /**
     * @brief Set a named vec4 uniform variable.
     * @param name Uniform variable name in the shader source.
     * @param value 4-component vector value to upload.
     */
    void setUniform(const std::string& name, const glm::vec4& value) const;

    /**
     * @brief Set a named mat3 uniform variable.
     * @param name Uniform variable name in the shader source.
     * @param value 3x3 matrix value to upload.
     */
    void setUniform(const std::string& name, const glm::mat3& value) const;

    /**
     * @brief Set a named mat4 uniform variable.
     * @param name Uniform variable name in the shader source.
     * @param value 4x4 matrix value to upload.
     */
    void setUniform(const std::string& name, const glm::mat4& value) const;

    /**
     * @brief Set a mat4 uniform by name (convenience alias for setUniform with glm::mat4).
     * @param name Uniform variable name in the shader source.
     * @param mat 4x4 matrix value to upload.
     */
    void setMat4(const std::string& name, const glm::mat4& mat) const;

    /**
     * @brief Return the underlying OpenGL program handle.
     * @return OpenGL program object ID.
     */
    GLuint id() const { return m_program; }

private:
    GLuint m_program = 0;

    /**
     * @brief Read a shader source file into memory.
     * @param path File path to the shader source.
     * @return File contents as a string.
     */
    std::string loadFile(const std::string& path) const;

    /**
     * @brief Compile one shader stage and return its OpenGL handle.
     * @param type Shader stage type (e.g. GL_VERTEX_SHADER or GL_FRAGMENT_SHADER).
     * @param src GLSL source code string to compile.
     * @return OpenGL shader object ID, or 0 on failure.
     */
    GLuint compileShader(GLenum type, const std::string& src) const;
};