#version 330 core
layout (location = 0) in vec3 vertexPosition;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec2 vertexTexCoord;

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec3 fragNormal;

uniform mat4 mvp;
uniform mat4 camera;    // matrix that stores the camera position etc.

void main()
{
    fragPosition    = vec3(mvp * vec4(vertexPosition, 1.0));
    fragNormal      = normalize(vec3(inverse(transpose(mvp)) * vec4(vertexNormal, 1.0)));
    fragTexCoord    = vertexTexCoord;
    
    gl_Position     = camera * vec4(fragPosition, 1.0);
}