#version 330 core

layout (location = 0) in vec3 position;
layout (location = 4) in vec4 joints;   // joint IDs.
layout (location = 5) in vec4 weights;  // joint weights.

uniform mat4 light;
uniform mat4 mvp;
uniform mat4 joint_matrices[100];       // array of joint transformations.

void main()
{
    mat4 skin = 
        weights.x * joint_matrices[int(joints.x)] +
        weights.y * joint_matrices[int(joints.y)] +
        weights.z * joint_matrices[int(joints.z)] +
        weights.w * joint_matrices[int(joints.w)];

    gl_Position = light * mvp * skin * vec4(position, 1.0);
}