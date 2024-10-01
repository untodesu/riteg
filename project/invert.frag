#version 330 core

out vec4 frag_Target;
uniform vec2 u_Resolution;
uniform sampler2D u_Input0;

void main(void)
{
    frag_Target.xyz = 1.0 - texture(u_Input0, gl_FragCoord.xy / u_Resolution).xyz;
    frag_Target.w = 1.0;
}
