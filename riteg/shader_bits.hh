#ifndef RITEG_SHADER_BITS_HH
#define RITEG_SHADER_BITS_HH 1
#pragma once

constexpr static const char *GLSL_VERSION_SHADER_BIT = R"glsl(
    #version 330 core
)glsl";

constexpr static const char *FRAG_RITEG_SHADER_BIT = R"glsl(
    #define RITEG 1
    #define RITEG_VERSION 25
)glsl";

constexpr static const char *FRAG_SHADERTOY_HEADER_SHADER_BIT = R"glsl(
    uniform vec3 iResolution;
    uniform float iTime;
    uniform float iTimeDelta;
    uniform float iFrame;
    uniform float iChannelTime[4];
    uniform vec4 iMouse;
    uniform vec4 iDate;
    uniform vec3 iChannelResolution[4];
    uniform sampler2D iChannel0;
    uniform sampler2D iChannel1;
    uniform sampler2D iChannel2;
    uniform sampler2D iChannel3;
)glsl";

constexpr static const char *FRAG_SHADERTOY_FOOTER_SHADER_BIT = R"glsl(
    out vec4 RITEG_fragColor;
    void main(void)
    {
        mainImage(RITEG_fragColor, gl_FragCoord.xy);
    }
)glsl";

constexpr static const char *VERT_ENTRYPOINT_SHADER_BIT = R"glsl(
    void main(void)
    {
        vec2 coords[4] = vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));
        gl_Position.xy = coords[gl_VertexID % 4];
        gl_Position.z = 0.0;
        gl_Position.w = 1.0;
    }
)glsl";

#endif /* RITEG_SHADER_BITS_HH */
