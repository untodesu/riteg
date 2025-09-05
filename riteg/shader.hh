#pragma once

#include "riteg/source.hh"

class Shader final : public Source {
public:
    constexpr static int CHANNEL0 = 0; // iChannel0
    constexpr static int CHANNEL1 = 1; // iChannel1
    constexpr static int CHANNEL2 = 2; // iChannel2
    constexpr static int CHANNEL3 = 3; // iChannel3

    constexpr static std::size_t MAX_CHANNELS = 4;

    static void init(void);
    static void deinit(void);
    static void beginFrame(void);

    explicit Shader(int width, int height, const std::string& source);
    virtual ~Shader(void) override;

    virtual GLuint get_texture(void) const override;
    virtual GLuint get_framebuffer(void) const override;
    virtual int get_texture_width(void) const override;
    virtual int get_texture_height(void) const override;
    virtual void render(const Timings& timings) override;

    void setChannel(int channel, const Source* source);

private:
    GLuint m_program;
    GLuint m_fragment_shader;

    int m_target_width;
    int m_target_height;
    GLuint m_target_fbo;
    GLuint m_target_texture;

    const Source* m_channels[MAX_CHANNELS];

    GLint u_iResolution;
    GLint u_iTime;
    GLint u_iTimeDelta;
    GLint u_iFrame;
    GLint u_iChannelTime[MAX_CHANNELS];
    GLint u_iMouse;
    GLint u_iDate;
    GLint u_iChannelResolution[MAX_CHANNELS];
    GLint u_iChannel[MAX_CHANNELS];
};
