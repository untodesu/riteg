#version 330 core

out vec4 frag_Target;
uniform vec2 u_Resolution;
uniform sampler2D u_Input0;

void main(void)
{
    frag_Target = texture(u_Input0, gl_FragCoord.xy / u_Resolution);
}
