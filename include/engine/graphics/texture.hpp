/* Start Header *****************************************************************/
/*!
\file   texture.hpp
\author Choi Meng Yew
\par    choi.m@digipen.edu
\date   24th September 2025
\brief
    Texture class is a lightweight RAII wrapper around an OpenGL texture object.
    It loads an image file using stb_image, creates a GPU texture, and manages
    its lifetime automatically.

    This class follows the Rule of 5 because it owns a GPU resource (GLuint).
    If the compiler were to auto-generate copy operations, multiple Texture
    objects could incorrectly share the same OpenGL ID. That would lead to
    double deletions or dangling texture handles (black quads, crashes, leaks).

    To prevent this:
      - Copy constructor and copy assignment perform a deep copy
        (reload the image from disk and create a new GPU texture).
      - Move constructor and move assignment are implemented so ownership of
        the GPU texture ID can be transferred safely.
      - Destructor ensures the OpenGL texture is deleted exactly once.

Intended Usage:
    Texture tex("assets/sprite.png");
    tex.Bind(0);

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#pragma once
#include <glad/glad.h>
#include <string>

class Texture {
public:
    Texture() : m_id(0), m_width(0), m_height(0), m_channels(0) {}

    // Constructor & Destructor
    Texture(const std::string& path);
    ~Texture();

    // Be wary of copy operations (cannot duplicate GPU handles safely; implement deep copy)
    Texture(const Texture& other);
    Texture& operator=(const Texture& other);

    // Move semantics (safe transfer of GPU ownership)
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    // API
    void Bind(unsigned int slot = 0) const;

    // Getters
    GLuint ID() const { return m_id; }
    int Width() const { return m_width; }
    int Height() const { return m_height; }

private:
    // Store path so we can reload during deep copy
    std::string m_path;

    GLuint m_id = 0;
    int m_width = 0;
    int m_height = 0;
    int m_channels = 0;

    // Helper for both ctor and copy assignment
    void loadFromFile(const std::string& path);
    void cleanup();
};