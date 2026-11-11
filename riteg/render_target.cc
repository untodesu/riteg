#include "riteg/pch.hh"

#include "riteg/render_target.hh"

namespace
{
GLenum to_internal_format(TextureFormat format)
{
    switch(format) {
        case TextureFormat::RGBA16F:
            return GL_RGBA16F;
        case TextureFormat::RGBA8:
            return GL_RGBA8;
        case TextureFormat::R16F:
            return GL_R16F;
        case TextureFormat::RG16F:
            return GL_RG16F;
    }

    return GL_RGBA16F;
}

GLenum to_pixel_format(TextureFormat format)
{
    switch(format) {
        case TextureFormat::RGBA16F:
        case TextureFormat::RGBA8:
            return GL_RGBA;
        case TextureFormat::R16F:
            return GL_RED;
        case TextureFormat::RG16F:
            return GL_RG;
    }

    return GL_RGBA;
}

GLenum to_pixel_type(TextureFormat format)
{
    switch(format) {
        case TextureFormat::RGBA8:
            return GL_UNSIGNED_BYTE;
        case TextureFormat::RGBA16F:
        case TextureFormat::R16F:
        case TextureFormat::RG16F:
            return GL_FLOAT;
    }

    return GL_FLOAT;
}

std::size_t bytes_per_pixel(TextureFormat format)
{
    switch(format) {
        case TextureFormat::RGBA16F:
            return 8;
        case TextureFormat::RGBA8:
            return 4;
        case TextureFormat::R16F:
            return 2;
        case TextureFormat::RG16F:
            return 4;
    }

    return 8;
}

GLenum to_filter(TextureFilter filter)
{
    switch(filter) {
        case TextureFilter::Linear:
            return GL_LINEAR;
        case TextureFilter::Nearest:
            return GL_NEAREST;
    }

    return GL_LINEAR;
}

GLenum to_wrap(TextureWrap wrap)
{
    switch(wrap) {
        case TextureWrap::Clamp:
            return GL_CLAMP_TO_EDGE;
        case TextureWrap::Repeat:
            return GL_REPEAT;
        case TextureWrap::Mirror:
            return GL_MIRRORED_REPEAT;
    }

    return GL_CLAMP_TO_EDGE;
}
} // namespace

RenderTarget::RenderTarget(const TargetKey& key) : m_key(key)
{
    assert(key.width > 0);
    assert(key.height > 0);

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(to_internal_format(key.format)), key.width, key.height, 0,
        to_pixel_format(key.format), to_pixel_type(key.format), nullptr);

    auto filter = to_filter(key.filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

    auto wrap = to_wrap(key.wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

    glGenFramebuffers(1, &m_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);

    riteg_force_assert_msg(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "incomplete render target framebuffer");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

RenderTarget::~RenderTarget(void)
{
    glDeleteFramebuffers(1, &m_framebuffer);
    glDeleteTextures(1, &m_texture);
}

GLuint RenderTarget::get_texture(void) const
{
    return m_texture;
}

GLuint RenderTarget::get_framebuffer(void) const
{
    return m_framebuffer;
}

int RenderTarget::get_width(void) const
{
    return m_key.width;
}

int RenderTarget::get_height(void) const
{
    return m_key.height;
}

const TargetKey& RenderTarget::get_key(void) const
{
    return m_key;
}

std::size_t RenderTarget::get_byte_size(void) const
{
    return static_cast<std::size_t>(m_key.width) * static_cast<std::size_t>(m_key.height) * bytes_per_pixel(m_key.format);
}

void RenderTarget::clear(void) const
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glViewport(0, 0, m_key.width, m_key.height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}
