#include "riteg/pch.hh"

#include "riteg/blank.hh"

namespace
{
constexpr int BLANK_WIDTH = 16;
constexpr int BLANK_HEIGHT = 16;
} // namespace

Blank::Blank(void) : Blank(0, 0, 0)
{
    // empty
}

Blank::Blank(int red, int green, int blue)
{
    assert(red >= 0 && red <= 255);
    assert(green >= 0 && green <= 255);
    assert(blue >= 0 && blue <= 255);

    std::byte pixels[BLANK_WIDTH * BLANK_HEIGHT * 4];

    for(int i = 0; i < BLANK_WIDTH * BLANK_HEIGHT; ++i) {
        pixels[i * 4 + 0] = static_cast<std::byte>(red);
        pixels[i * 4 + 1] = static_cast<std::byte>(green);
        pixels[i * 4 + 2] = static_cast<std::byte>(blue);
        pixels[i * 4 + 3] = static_cast<std::byte>(255);
    }

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, BLANK_WIDTH, BLANK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

Blank::~Blank(void)
{
    glDeleteTextures(1, &m_texture);
}

GLuint Blank::get_texture(void) const
{
    return m_texture;
}

GLuint Blank::get_framebuffer(void) const
{
    return 0;
}

int Blank::get_texture_width(void) const
{
    return BLANK_WIDTH;
}

int Blank::get_texture_height(void) const
{
    return BLANK_HEIGHT;
}

void Blank::render(const Timings& timings)
{
    // empty
}
