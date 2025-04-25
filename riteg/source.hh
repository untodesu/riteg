#ifndef RITEG_SOURCE_HH
#define RITEG_SOURCE_HH 1
#pragma once

struct Timings;

/**
 * Each rendering pass may require a bunch of inputs; each input
 * can be either a loaded texture, a pre-generated texture or an
 * another rendering pass's output; consequently, this is a base class
 * for anything that can and does produce a texture
 */
class Source {
public:
    virtual ~Source(void) = default;
    virtual GLuint get_texture(void) const = 0;
    virtual GLuint get_framebuffer(void) const = 0;
    virtual int get_texture_width(void) const = 0;
    virtual int get_texture_height(void) const = 0;
    virtual void render(const Timings &timings) = 0;
};

#endif /* RITEG_SOURCE_HH */
