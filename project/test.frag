#version 330 core

out vec4 frag_Target;
uniform vec2 u_Resolution;
uniform float u_Params[1];
uniform float u_Time;

void main(void)
{
    vec2 uv = gl_FragCoord.xy / u_Resolution;
    frag_Target = vec4(floor(u_Params[0] * uv) / u_Params[0], 1.0, 1.0);
    frag_Target.xyz *= 0.5 + 0.5 * cos(u_Time);
}
