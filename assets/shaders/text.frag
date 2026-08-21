#version 330 core

in vec2 frag_tex_coord;
uniform sampler2D tex0;
layout (location = 0) out vec4 final_color;

void main()
{
    final_color = texture(tex0, frag_tex_coord);
}