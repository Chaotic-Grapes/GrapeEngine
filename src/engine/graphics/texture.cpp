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

\note
Textures are uploaded using the **GL_SRGB8_ALPHA8** internal format, which stores
RGB channels in sRGB color space and performs automatic sRGB to linear conversion
when sampled in shaders. This ensures physically correct lighting and tone
mapping throughout the pipeline.
Use **GL_RGBA8** instead only for data or non-color textures (e.g., normal maps,
roughness maps) where gamma correction must not be applied.
*/
/* End Header *******************************************************************/

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "graphics/texture.hpp"
#include "services/ResourceManager.h"
#include <iostream>

// ============================================================================
// Loads texture data from disk using stb_image and uploads it to the GPU.
// Also configures filtering and wrapping parameters for 2D sampling.
// ============================================================================
void Texture::loadFromFile(const std::string& path) {
    m_path = path; // Store file path for deep copies (reloads on copy/assignment)

    // stb_image loads images flipped vertically by default for OpenGL
    // (since OpenGL texture coordinates (0,0) start at bottom-left)
    stbi_set_flip_vertically_on_load(true);

    int width, height, channels;

    // Load image data from disk into CPU memory (RGBA8 format enforced)
    // This decodes the file (PNG, JPG, etc.) into a simple 8-bit-per-channel array.
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return;
    }

    // Store metadata for later use (debug, atlasing, or reloading)
    m_width = width;
    m_height = height;
    m_channels = channels;

    // Generate a new texture object and bind it so following calls affect it
    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);

    // ------------------------------------------------------------
    // Configure texture sampling and wrapping behavior
    // ------------------------------------------------------------
    // NEAREST filtering keeps pixel edges sharp � ideal for sprites or pixel art.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // Clamp UVs beyond [0,1] to edge texel � prevents bleeding between atlas regions.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // ------------------------------------------------------------
    // Upload pixel data from CPU to GPU memory
    // ------------------------------------------------------------
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8,
        m_width, m_height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, data);

    // Unbind the texture to avoid accidental modification
    glBindTexture(GL_TEXTURE_2D, 0);

    // Free the CPU-side buffer since the GPU now has its own copy
    stbi_image_free(data);
}

// ============================================================================
// Deletes the GPU texture resource if it exists to prevent memory leaks.
// Called automatically by destructor or before reloading a texture.
// ============================================================================
void Texture::cleanup() {
    if (m_id) {
        glDeleteTextures(1, &m_id);
        m_id = 0;
    }
}

// ============================================================================
// Constructs a texture and loads image data if a valid file path is provided.
// ============================================================================
Texture::Texture(const std::string& path) {
    if (!path.empty()) {
        loadFromFile(path);
    }
}

// ============================================================================
// Deep copy constructor: reloads the texture from the original file path.
// Each copy owns its own GPU texture resource.
// ============================================================================
Texture::Texture(const Texture& other) {
    if (!other.m_path.empty()) {
        loadFromFile(other.m_path);
    }
}

// ============================================================================
// Deep copy assignment: releases existing texture and reloads from file path.
// ============================================================================
Texture& Texture::operator=(const Texture& other) {
    if (this != &other) {
        cleanup();
        if (!other.m_path.empty()) {
            loadFromFile(other.m_path);
        }
    }
    return *this;
}

// ============================================================================
// Move constructor: transfers GPU ownership without reloading the texture.
// The source object is reset to a safe, empty state.
// ============================================================================
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

// ============================================================================
// Move assignment: releases current texture, then takes ownership of another.
// Prevents double-deletion by resetting the source texture�s state.
// ============================================================================
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

// ============================================================================
// Destructor: automatically releases the GPU texture resource.
// Ensures cleanup occurs exactly once when the texture goes out of scope.
// ============================================================================
Texture::~Texture() {
    cleanup();
}

// ============================================================================
// Binds the texture to a specified texture slot for sampling in shaders.
// ============================================================================
void Texture::Bind(unsigned int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_id);
}