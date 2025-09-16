#pragma once
#include <glad/glad.h>
#include <string>

class Texture {
public:
    Texture(const std::string& path);
    ~Texture();

    void Bind(unsigned int slot = 0) const;

    GLuint ID() const { return m_id; }
    int Width() const { return m_width; }
    int Height() const { return m_height; }

private:
    GLuint m_id = 0;
    int m_width = 0;
    int m_height = 0;
    int m_channels = 0;
};