/* Start Header *****************************************************************/
/*!
\file   texture.cpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   24th September 2025
\brief
Implements the Texture class, a lightweight RAII wrapper around an OpenGL
texture object. It loads images via stb_image, creates and configures the
GPU texture, and ensures proper cleanup of the OpenGL resource.

Features:
- Loads textures from disk with vertical flip enabled.
- Configures filtering and wrapping parameters.
- Supports deep copy semantics by reloading from the original file path.
- Provides move semantics for safe transfer of ownership.
- Ensures GPU texture deletion exactly once in the destructor.
*/
/* End Header *******************************************************************/

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "graphics/texture.hpp"
#include "services/ResourceManager.h"
#include <iostream>

// Private Helpers
void Texture::loadFromFile(const std::string& path) {
    m_path = path; // keep for deep copies

    stbi_set_flip_vertically_on_load(true);

    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return;
    }

    m_width = width;
    m_height = height;
    m_channels = channels;

    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);

    // Parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Upload pixels
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
        m_width, m_height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, data);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
}

void Texture::cleanup() {
    if (m_id) {
        glDeleteTextures(1, &m_id);
        m_id = 0;
    }
}

// Constructors / Assignment
Texture::Texture(const std::string& path) {
    if (!path.empty()) {
        loadFromFile(path);
    }
}

// Deep copy constructor
Texture::Texture(const Texture& other) {
    if (!other.m_path.empty()) {
        loadFromFile(other.m_path);
    }
}

// Deep copy assignment
Texture& Texture::operator=(const Texture& other) {
    if (this != &other) {
        cleanup();
        if (!other.m_path.empty()) {
            loadFromFile(other.m_path);
        }
    }
    return *this;
}

Texture::Texture(Texture&& other) noexcept {
    m_id = other.m_id;
    m_width = other.m_width;
    m_height = other.m_height;
    m_channels = other.m_channels;

    other.m_id = 0;
    other.m_width = 0;
    other.m_height = 0;
    other.m_channels = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        // Free current GPU resource
        if (m_id) {
            glDeleteTextures(1, &m_id);
        }

        // Transfer ownership
        m_id = other.m_id;
        m_width = other.m_width;
        m_height = other.m_height;
        m_channels = other.m_channels;

        other.m_id = 0;
        other.m_width = 0;
        other.m_height = 0;
        other.m_channels = 0;
    }
    return *this;
}

Texture::~Texture() {
    cleanup();
}

void Texture::Bind(unsigned int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_id);
}