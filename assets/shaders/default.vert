#version 330 core

layout (location = 0) in vec3 vertex_position;  // position of vertex.
layout (location = 1) in vec3 vertex_normal;    // normal of the vertex.
layout (location = 2) in vec3 vertex_color;     // colour stored in the vertex.
layout (location = 3) in vec2 vertex_texcoord;  // vertex texture coordinates.
layout (location = 4) in vec4 joints;           // joint IDs.
layout (location = 5) in vec4 weights;          // joint weights.

#define NUM_CASCADES 3
#define MAX_JOINTS 100

uniform mat4 mvp;                               // projection matrix that stores the position, rotation, and scale of a mesh. 
uniform mat4 camera;                            // view matrix that stores the camera position etc.
uniform mat4 light[NUM_CASCADES];               // light matrix from shadow, one for each cascade.
uniform mat4 joint_matrices[MAX_JOINTS];        // array of joint transformations.

// out vec3 frag_position;                         // outputs the current position for the Fragment Shader
out vec3 frag_normal;                           // outputs normal
out vec3 frag_color;                            // outputs color
out vec2 frag_texcoord;                         // outputs texture coordinates
out vec4 frag_shadowcoords[NUM_CASCADES];       // outputs position respective to light.

void main()
{
    mat4 skin = 
        weights.x * joint_matrices[int(joints.x)] +
        weights.y * joint_matrices[int(joints.y)] +
        weights.z * joint_matrices[int(joints.z)] +
        weights.w * joint_matrices[int(joints.w)];

    vec4 local_position = mvp * vec4(vertex_position, 1.0);
    gl_Position         = camera * local_position;
    frag_normal         = normalize(vec3(inverse(transpose(mvp)) * vec4(vertex_normal, 1.0)));
    frag_color          = vertex_color;
    frag_texcoord       = vertex_texcoord;


    // shadowmap.
    for (int i = 0 ; i < NUM_CASCADES ; ++i)
    {
        frag_shadowcoords[i] = light[i] * local_position;
    }
}