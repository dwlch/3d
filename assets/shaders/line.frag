#version 330 core

// input vertex attributes (from vertex shader)
in vec3 fragPosition;   // fragment position.
in vec3 fragNormal;     // fragment normal.
in vec3 fragColor;      // colour of the fragment.
in vec2 fragTexCoord;   // fragment texture coordinates.
in vec4 fragPosLight;   // positions from from light perspective.

uniform vec3 diffuse;       // base colour.

out vec4 finalColor;        // output fragment color

void main()
{
    finalColor = vec4(5.0);
}