/* Start Header *****************************************************************/
/*!
\file   font.hpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   3rd October 2025
\brief
Declares the Font class and supporting Glyph struct for text rendering.
Each glyph stores its metrics (size, bearing, advance) and UV coordinates
into an SDF-based atlas. The Font class handles loading a font file via
stb_truetype, generating a signed distance field (SDF) atlas, and managing
the associated OpenGL texture.

Features:
- Loads font data from disk into memory.
- Builds an SDF atlas containing all required glyphs.
- Stores glyph metrics for layout (size, bearing, advance).
- Provides access to atlas texture, dimensions, and line height.
- Move-only type to safely manage GPU texture ownership.
*/
/* End Header *******************************************************************/

#pragma once
#include "Export.h"
#include <string>
#include <map>
#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>

// Forward declare stb_truetype type (avoid header dependency)
struct stbtt_fontinfo;

// Represents a single glyph's placement inside the atlas
struct Glyph {
    glm::ivec2 size{};          // Glyph size in pixels
    glm::ivec2 bearing{};       // Offset from baseline
    unsigned int advance{};     // Horizontal spacing to next glyph (changed from float)
    glm::vec4 uv{};             // UV coords in atlas {u0, v0, u1, v1}
};

class GRAPEENGINE_API Font {
public:
    Font(const std::string& path, int pixelSize);
    ~Font();

    // Move only (fonts manage GPU resources)
    Font(Font const&) = delete;
    Font& operator=(Font const&) = delete;
    Font(Font&& other) noexcept;
    Font& operator=(Font&& other) noexcept;

    Glyph const& getGlyph(char c) const;
    GLuint getAtlasTexture() const { return m_atlasTex; }
    glm::ivec2 getAtlasSize() const { return m_atlasSize; }
    int getPixelSize() const { return m_pixelSize; }
    float getLineHeight() const { return m_lineHeight; }

private:
    void cleanup();
    void loadAtlas();   // builds the SDF atlas + fills m_glyphs

    std::string m_path{};
    int m_pixelSize = 0;
    float m_lineHeight = 0.0f;

    // Font data kept in memory for stb_truetype
    std::vector<unsigned char> m_fontBuffer;

    GLuint m_atlasTex = 0;                  // OpenGL texture containing atlas
    glm::ivec2 m_atlasSize{ 0, 0 };         // Width/height of atlas
    std::map<char, Glyph> m_glyphs;         // Metadata + UV per glyph
};