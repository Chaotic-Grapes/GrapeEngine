/* Start Header *****************************************************************/
/*!
\file   texture.hpp
\author Choi Meng Yew (100%)
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
#include "Export.h"
#include <glad/glad.h>
#include <string>

class GRAPEENGINE_API Texture {
public:
    Texture() : m_id(0), m_width(0), m_height(0), m_channels(0) {}

    /**
     * @brief Load a texture from disk and create a GPU texture object.
     * @param path File path to the image asset.
     */
    Texture(const std::string& path);
    ~Texture();

    /**
     * @brief Deep-copy constructor; reloads the image from disk and creates a new GPU texture.
     * @param other Source texture to copy from.
     */
    Texture(const Texture& other);

    /**
     * @brief Deep-copy assignment; reloads the image from disk and creates a new GPU texture.
     * @param other Source texture to copy from.
     * @return Reference to this texture after assignment.
     */
    Texture& operator=(const Texture& other);

    /**
     * @brief Move constructor; transfers GPU texture ownership without reloading.
     * @param other Source texture to move from (left in a null state).
     */
    Texture(Texture&& other) noexcept;

    /**
     * @brief Move assignment; transfers GPU texture ownership without reloading.
     * @param other Source texture to move from (left in a null state).
     * @return Reference to this texture after assignment.
     */
    Texture& operator=(Texture&& other) noexcept;

    /**
     * @brief Bind this texture to the given texture unit slot.
     * @param slot Texture unit index (0-based) to bind to.
     */
    void Bind(unsigned int slot = 0) const;

    /**
     * @brief Return the underlying OpenGL texture handle.
     * @return OpenGL texture object ID.
     */
    GLuint ID() const { return m_id; }

    /**
     * @brief Return the texture width in pixels.
     * @return Texture width in pixels.
     */
    int Width() const { return m_width; }

    /**
     * @brief Return the texture height in pixels.
     * @return Texture height in pixels.
     */
    int Height() const { return m_height; }

private:
    // Store path so we can reload during deep copy
    std::string m_path;

    GLuint m_id = 0;
    int m_width = 0;
    int m_height = 0;
    int m_channels = 0;

    /**
     * @brief Load an image from disk, create a GPU texture, and populate member fields.
     * @param path File path to the image asset.
     */
    void loadFromFile(const std::string& path);

    /**
     * @brief Delete the owned OpenGL texture and reset GPU state to zero.
     */
    void cleanup();
};