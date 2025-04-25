#ifndef RITEG_BLANK_HH
#define RITEG_BLANK_HH 1
#pragma once

#include "riteg/source.hh"

class Blank final : public Source {
public:
    explicit Blank(void);
    explicit Blank(int red, int green, int blue);
    virtual ~Blank(void) override;

    virtual GLuint get_texture(void) const override;
    virtual GLuint get_framebuffer(void) const override;
    virtual int get_texture_width(void) const override;
    virtual int get_texture_height(void) const override;
    virtual void render(const Timings &timings) override;

private:
    GLuint m_texture;
};

#endif /* RITEG_BLANK_HH */
