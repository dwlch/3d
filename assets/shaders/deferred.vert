#version 330 core

// input vertex attributes
layout (location = 0) in vec3 vertexPosition;     // position of vertex.
layout (location = 1) in vec2 vertexTexCoord;     // vertex texture coordinates.

out vec2 fragTexCoord;

void main()
{
    fragTexCoord    = vertexTexCoord;
	gl_Position     = vec4(vertexPosition, 1.0); // position from camera.
}