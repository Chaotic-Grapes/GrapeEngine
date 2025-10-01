#include "../include/graphics/font.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace {
    constexpr int AtlasWidth = 1024;
    constexpr int AtlasHeight = 1024;

    constexpr int SDFPadding = 8;
    constexpr int SDFOnEdgeValue = 180;
    constexpr float SDFPixelDistScale =
        static_cast<float>(SDFOnEdgeValue) / static_cast<float>(SDFPadding);
}

Font::Font(const std::string& path, int pixelSize)
    : m_path(path), m_pixelSize(pixelSize)
{
    // 1) Read font file into memory (persistent buffer)
    std::ifstream file(m_path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("[Font] ERROR: Cannot open font file: " + m_path);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    m_fontBuffer.resize(size);
    if (!file.read(reinterpret_cast<char*>(m_fontBuffer.data()), size)) {
        throw std::runtime_error("[Font] ERROR: Failed to read font file: " + m_path);
    }

    // Build SDF atlas
    loadAtlas();
}

Font::~Font() {
    cleanup();
}

Font::Font(Font&& other) noexcept
    : m_path(std::move(other.m_path))
    , m_fontBuffer(std::move(other.m_fontBuffer))
    , m_pixelSize(other.m_pixelSize)
    , m_lineHeight(other.m_lineHeight)
    , m_atlasTex(other.m_atlasTex)
    , m_atlasSize(other.m_atlasSize)
    , m_glyphs(std::move(other.m_glyphs))
{
    other.m_pixelSize = 0;
    other.m_lineHeight = 0.0f;
    other.m_atlasTex = 0;
    other.m_atlasSize = { 0, 0 };
}

Font& Font::operator=(Font&& other) noexcept {
    if (this != &other) {
        cleanup();

        m_path = std::move(other.m_path);
        m_fontBuffer = std::move(other.m_fontBuffer);
        m_pixelSize = other.m_pixelSize;
        m_lineHeight = other.m_lineHeight;
        m_atlasTex = other.m_atlasTex;
        m_atlasSize = other.m_atlasSize;
        m_glyphs = std::move(other.m_glyphs);

        other.m_pixelSize = 0;
        other.m_lineHeight = 0.0f;
        other.m_atlasTex = 0;
        other.m_atlasSize = { 0, 0 };
    }
    return *this;
}

void Font::cleanup() {
    if (m_atlasTex) {
        glDeleteTextures(1, &m_atlasTex);
        m_atlasTex = 0;
    }
    m_glyphs.clear();
}

void Font::loadAtlas() {
    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, m_fontBuffer.data(),
        stbtt_GetFontOffsetForIndex(m_fontBuffer.data(), 0))) {
        throw std::runtime_error("[Font] ERROR: stb_truetype init failed");
    }

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    float scale = stbtt_ScaleForPixelHeight(&font, static_cast<float>(m_pixelSize));
    m_lineHeight = (ascent - descent + lineGap) * scale;

    m_atlasSize = { AtlasWidth, AtlasHeight };
    std::vector<unsigned char> atlasData(AtlasWidth * AtlasHeight, 0);

    int xOffset = 0, yOffset = 0, rowHeight = 0;

    for (unsigned char c = 32; c < 127; ++c) {
        if (c == ' ') {
            int advance, lsb;
            stbtt_GetCodepointHMetrics(&font, c, &advance, &lsb);

            Glyph glyph;
            glyph.size = { 0, 0 };
            glyph.bearing = { 0, 0 };
            glyph.advance = static_cast<unsigned int>(advance * scale);
            glyph.uv = { 0, 0, 0, 0 };
            m_glyphs[c] = glyph;
            continue;
        }

        int w, h, xoff, yoff;
        unsigned char* sdf = stbtt_GetCodepointSDF(
            &font, scale, c,
            SDFPadding,
            SDFOnEdgeValue,
            SDFPixelDistScale,
            &w, &h, &xoff, &yoff
        );

        if (!sdf) {
            std::cerr << "[Font] WARNING: failed to generate SDF for '" << c << "'\n";
            continue;
        }

        if (xOffset + w >= AtlasWidth) {
            xOffset = 0;
            yOffset += rowHeight + 1;
            rowHeight = 0;
        }
        if (yOffset + h >= AtlasHeight) {
            STBTT_free(sdf, nullptr);
            throw std::runtime_error("[Font] ERROR: Atlas overflow, increase atlas size!!!");
        }

        // Copy each glyph row into the atlas using memcpy instead of an inner x-loop.
        // This is faster than assigning pixels one at a time because memcpy performs
        // a contiguous block transfer per row (often SIMD-optimized), reducing loop
        // overhead and improving cache efficiency.
        for (int y = 0; y < h; ++y) {
            std::memcpy(
                &atlasData[(yOffset + y) * AtlasWidth + xOffset],
                &sdf[y * w],
                w
            );
        }

        int advance, lsb;
        stbtt_GetCodepointHMetrics(&font, c, &advance, &lsb);

        Glyph glyph;
        glyph.size = { w, h };
        glyph.bearing = { xoff, -yoff };
        glyph.advance = static_cast<unsigned int>(advance * scale);
        glyph.uv = {
            static_cast<float>(xOffset) / AtlasWidth,
            static_cast<float>(yOffset + h) / AtlasHeight,
            static_cast<float>(xOffset + w) / AtlasWidth,
            static_cast<float>(yOffset) / AtlasHeight
        };

        m_glyphs[c] = glyph;

        xOffset += w + 1;
        rowHeight = std::max(rowHeight, h);

        STBTT_free(sdf, nullptr);
    }

    glGenTextures(1, &m_atlasTex);
    glBindTexture(GL_TEXTURE_2D, m_atlasTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, AtlasWidth, AtlasHeight, 0,
        GL_RED, GL_UNSIGNED_BYTE, atlasData.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    std::cout << "[Font] SDF Atlas built: " << AtlasWidth << "x" << AtlasHeight
        << " with " << m_glyphs.size() << " glyphs\n";
}

const Glyph& Font::getGlyph(char c) const {
    auto it = m_glyphs.find(c);
    if (it == m_glyphs.end()) {
        static Glyph empty{ {0,0}, {0,0}, 0, {0,0,0,0} };
        return empty;
    }
    return it->second;
}