#version 330 core

// input vertex attributes
layout (location = 0) in vec3 vertexPosition;     // position of vertex.
layout (location = 1) in vec3 vertexNormal;       // normal of the vertex.
layout (location = 2) in vec3 vertexColor;        // colour stored in the vertex.
layout (location = 3) in vec2 vertexTexCoord;     // vertex texture coordinates.

#define NUM_CASCADES 3

// input uniform values
uniform mat4 mvp;                   // matrix that stores the position, rotation, and scale of a mesh.
uniform mat4 camera;                // matrix that stores the camera position etc.
uniform mat4 light[NUM_CASCADES];   // light matrix from shadow, one for each cascade.

out vec3 fragPosition;  // outputs the current position for the Fragment Shader
out vec3 fragNormal;    // outputs normal
out vec3 fragColor;     // outputs color
out vec2 fragTexCoord;  // outputs texture coordinates
out vec4 fragPosLight[NUM_CASCADES];  // outputs position respective to light.
out float ClipSpacePosZ;

void main()
{
    // position with respect to model matrix.
    fragPosition    = vec3(mvp * vec4(vertexPosition, 1.0));
    fragNormal      = normalize(vec3(inverse(transpose(mvp)) * vec4(vertexNormal, 1.0)));
    fragColor       = vertexColor;
    fragTexCoord    = vertexTexCoord;

    for (int i = 0 ; i < NUM_CASCADES ; i++)
    {
        fragPosLight[i] = light[i] * vec4(fragPosition, 1.0);
    }

	gl_Position     = camera * vec4(fragPosition, 1.0); // position from camera.
    // ClipSpacePosZ   = gl_Position.z;
}