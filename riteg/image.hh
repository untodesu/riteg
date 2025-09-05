#pragma once

#include "riteg/source.hh"

class Image final : public Source {
public:
    explicit Image(void);
    virtual ~Image(void) override;

    virtual GLuint get_texture(void) const override;
    virtual GLuint get_framebuffer(void) const override;
    virtual int get_texture_width(void) const override;
    virtual int get_texture_height(void) const override;
    virtual void render(const Timings& timings) override;

    bool load_RGBA(const std::filesystem::path& path);

private:
    GLuint m_texture;
    int m_texture_width;
    int m_texture_height;
};
