#version 330 core

layout (location = 0) in vec3 position;

uniform mat4 light;
uniform mat4 mvp;

void main()
{
    gl_Position = light * mvp * vec4(position, 1.0);
}