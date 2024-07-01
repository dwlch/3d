#version 330 core

// input vertex attributes (from vertex shader)
in vec2 fragTexCoord;   // fragment texture coordinates.

uniform sampler2D g_position;
uniform sampler2D g_normal;
uniform sampler2D g_albedo;
uniform vec3 light_pos;

out vec4 final_color;            // output fragment color.

void main()
{
    vec3 FragPos    = texture(g_position,   fragTexCoord).rgb;
    vec3 Normal     = texture(g_normal,     fragTexCoord).rgb;
    vec3 Diffuse    = texture(g_albedo,     fragTexCoord).rgb;

    final_color = vec4(Diffuse * 0.1, 1.0);
}

