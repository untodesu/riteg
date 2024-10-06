#version 330 core

out vec4 frag_Target;
uniform vec2 iResolution;
uniform sampler2D iChannel[1];

void main(void)
{
    frag_Target.xyz = 1.0 - texture(iChannel[0], gl_FragCoord.xy / iResolution).xyz;
    frag_Target.w = 1.0;
}
