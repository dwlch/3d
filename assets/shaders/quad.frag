#version 330 core

in vec2 fragTexCoord;
uniform sampler2D tex0;

out vec4 finalColor;

void main(void) 
{
    finalColor = texture(tex0, fragTexCoord);
}