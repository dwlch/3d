#version 330 core

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;

layout (location = 0) out vec3 g_position;
layout (location = 1) out vec3 g_normal;
layout (location = 2) out vec4 g_albedo;

uniform sampler2D tex0;

void main()
{    
    // store the fragment position vector in the first gbuffer texture
    g_position  = fragPosition;
    // also store the per-fragment normals into the gbuffer
    g_normal    = fragNormal;
    // and the diffuse per-fragment color
    g_albedo    = texture(tex0, fragTexCoord);
}  