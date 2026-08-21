#version 330 core

layout (location = 0) in vec3 vertex_position;  // position of vertex.
uniform mat4 mvp;
uniform mat4 view;

out vec3 texcoord0;

void main()
{
    // vec4 view_pos   = view * vec4(vertex_position, 1.0);
    // gl_Position     = view_pos.xyzw;

    vec4 view_pos   = view * mvp * vec4(vertex_position, 1.0);
    gl_Position     = view_pos.xyzw;


    texcoord0       = vertex_position;
}