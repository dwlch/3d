#version 330 core

layout (location = 0) in vec2 vertex_position;
layout (location = 1) in vec2 vertex_texcoord;

out vec2 frag_tex_coord;

void main()
{
    gl_Position     = vec4(vertex_position, 0.0, 1.0);
    frag_tex_coord  = vertex_texcoord;
}