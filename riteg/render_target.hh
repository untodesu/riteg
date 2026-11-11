#pragma once

enum class TextureFormat {
    RGBA16F,
    RGBA8,
    R16F,
    RG16F,
};

enum class TextureFilter {
    Linear,
    Nearest,
};

enum class TextureWrap {
    Clamp,
    Repeat,
    Mirror,
};

struct TargetKey final {
    int width;
    int height;
    TextureFormat format;
    TextureFilter filter;
    TextureWrap wrap;

    auto operator<=>(const TargetKey&) const = default;
};

class RenderTarget final {
public:
    explicit RenderTarget(const TargetKey& key);
    ~RenderTarget(void);

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    GLuint get_texture(void) const;
    GLuint get_framebuffer(void) const;
    int get_width(void) const;
    int get_height(void) const;
    const TargetKey& get_key(void) const;
    std::size_t get_byte_size(void) const;

    void clear(void) const;

private:
    TargetKey m_key;
    GLuint m_texture = 0;
    GLuint m_framebuffer = 0;
};
