#version 330 core

in vec2 fragTexCoord;
uniform sampler2D tex0;

out vec4 finalColor;

void main(void) 
{
    // finalColor = texture(tex0, fragTexCoord);
    vec3 col_a = vec3(1.5, 1.6, 1.7);
    vec3 col_b = vec3(1.1, 1.9, 1.2);


    finalColor = vec4(vec3(col_a * fragTexCoord.x) + vec3(col_b * fragTexCoord.y), 1.0);
}