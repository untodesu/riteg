#include "riteg/pch.hh"
#include "riteg/image.hh"

Image::Image(void) : m_texture_width(0), m_texture_height(0)
{
    glGenTextures(1, &m_texture);
}

Image::~Image(void)
{
    glDeleteTextures(1, &m_texture);
}

GLuint Image::get_texture(void) const
{
    return m_texture;
}

GLuint Image::get_framebuffer(void) const
{
    return 0;
}

int Image::get_texture_width(void) const
{
    return m_texture_width;
}

int Image::get_texture_height(void) const
{
    return m_texture_height;
}

void Image::render(const Timings &timings)
{
    // empty
}

bool Image::load_RGBA(const std::filesystem::path &path)
{
    assert(!path.empty());

    stbi_set_flip_vertically_on_load(true);

    if(auto pixels = stbi_load(path.string().c_str(), &m_texture_width, &m_texture_height, nullptr, STBI_rgb_alpha)) {
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_texture_width, m_texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(pixels);
        return true;
    }

    riteg_warning << "Image: " << path.string() << ": " << stbi_failure_reason() << std::endl;

    return false;
}
