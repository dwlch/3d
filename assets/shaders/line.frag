#version 330 core

uniform vec3 albedo;    // base colour, used for 'ingame' colours.

out vec4 final_color;

void main()
{
    vec3 colour = vec3(10.0, 10.0, 10.0);
    final_color = vec4(albedo, 1.0);
}