#include "../include/graphics/font.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

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
    // 2) Init stb_truetype
    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, m_fontBuffer.data(),
        stbtt_GetFontOffsetForIndex(m_fontBuffer.data(), 0))) {
        throw std::runtime_error("[Font] ERROR: stb_truetype init failed");
    }

    // Line metrics
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    float scale = stbtt_ScaleForPixelHeight(&font, (float)m_pixelSize);
    m_lineHeight = (ascent - descent + lineGap) * scale;

    // 3) Bake SDF atlas
    const int atlasW = 1024;
    const int atlasH = 1024;
    m_atlasSize = { atlasW, atlasH };

    std::vector<unsigned char> atlasData(atlasW * atlasH, 0);

    int xOffset = 0, yOffset = 0, rowHeight = 0;

    for (unsigned char c = 32; c < 127; ++c) {
        // Space handling
        if (c == ' ') {
            int advance, lsb;
            stbtt_GetCodepointHMetrics(&font, c, &advance, &lsb);

            Glyph glyph;
            glyph.size = { 0, 0 };
            glyph.bearing = { 0, 0 };
            glyph.advance = (unsigned int)(advance * scale);
            glyph.uv = { 0, 0, 0, 0 };

            m_glyphs[c] = glyph;
            continue;
        }

        int w, h, xoff, yoff;
        unsigned char* sdf = stbtt_GetCodepointSDF(
            &font, scale, c,
            8,             // padding
            180,           // on-edge value
            180.0f / 8.0f, // pixel_dist_scale
            &w, &h, &xoff, &yoff
        );

        if (!sdf) {
            std::cerr << "[Font] WARNING: failed to generate SDF for '" << c << "'\n";
            continue;
        }

        // Atlas row management
        if (xOffset + w >= atlasW) {
            xOffset = 0;
            yOffset += rowHeight + 1;
            rowHeight = 0;
        }
        if (yOffset + h >= atlasH) {
            free(sdf);
            throw std::runtime_error("[Font] ERROR: Atlas overflow, increase atlas size!!!");
        }

        // Copy glyph into atlas
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                atlasData[(yOffset + y) * atlasW + (xOffset + x)] =
                    sdf[y * w + x];
            }
        }

        // Metrics
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&font, c, &advance, &lsb);

        Glyph glyph;
        glyph.size = { w, h };
        glyph.bearing = { xoff, -yoff };
        glyph.advance = (unsigned int)(advance * scale);
        glyph.uv = {
            (float)xOffset / atlasW,
            (float)(yOffset + h) / atlasH,
            (float)(xOffset + w) / atlasW,
            (float)yOffset / atlasH
        };

        m_glyphs[c] = glyph;

        xOffset += w + 1;
        rowHeight = std::max(rowHeight, h);

        free(sdf);
    }

    // 4) Upload atlas to OpenGL
    glGenTextures(1, &m_atlasTex);
    glBindTexture(GL_TEXTURE_2D, m_atlasTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasW, atlasH, 0,
        GL_RED, GL_UNSIGNED_BYTE, atlasData.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    std::cout << "[Font] SDF Atlas built: " << atlasW << "x" << atlasH
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