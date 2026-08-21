#version 330 core

in vec3 texcoord0;

layout (location = 0) out vec4 final_color;
layout (location = 1) out vec4 bright_color;

uniform samplerCube cubemap_texture;

void main()
{
    float intensity = 2.5;
    final_color = texture(cubemap_texture, texcoord0) * vec4(vec3(intensity), 1.0);

    vec3 threshold      = vec3(0.2126, 0.7152, 0.0722);
    float brightness    = dot(final_color.rgb, threshold);
    if (brightness > 1.0)
    {
        bright_color = vec4(final_color.rgb, 1.0);
    }
    else
    {
        bright_color = vec4(vec3(0.0), 1.0);
    }
}