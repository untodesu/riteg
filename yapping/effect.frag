#version 330 core

// Sets the display name for the effect
#pragma effect Desaturate

// Sets input values for vertex outputs
//  layout(location = 0) in vec2 vs_Position
//  layout(location = 1) in vec2 vs_TexCoord
#pragma vert vs_Position vs_TexCoord

// Specifies an input texture
//  uniform sampler2D u_Texture1;
//  uniform vec2 u_Texture1_Size;
#pragma source u_Texture1

// Specifies an output texture
//  layout(location = 0) out vec4 frag_Target;
//  uniform vec2 frag_Target_Size;
#pragma output frag_Target 1.0 1.0

// Specifies effect parameters; in C++ these
// are parsed and defined as a bunch of InputFloat's
//  uniform float u_Params[3];
#pragma params u_Params[3]

in vec2 vs_Position;
in vec2 vs_TexCoord;

void main(void)
{
    vec4 color = texture(u_Texture1, vs_TexCoord);
    float value = dot(color.xyz, vec3(u_Params[0], u_Params[1], u_Params[2]));
    frag_Target = vec4(value, value, value, color.w);
}
