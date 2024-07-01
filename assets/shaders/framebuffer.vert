#version 330 core

in vec2 vertexPosition;
in vec2 vertexTexCoord;

out vec2 fragTexCoord;

void main()
{
    gl_Position     = vec4(vertexPosition.x, vertexPosition.y, 0.0, 1.0); 
    fragTexCoord    = vertexTexCoord;
}  