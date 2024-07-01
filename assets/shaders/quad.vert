#version 330 core

in vec2 vertexPosition;
in vec2 vertexTexCoord;

uniform float window_width;
uniform float window_height;
uniform float img_width;
uniform float img_height;
uniform float x_pos;
uniform float y_pos;
uniform float scale;

out vec2 fragTexCoord;

void main()
{
    float x = vertexPosition.x * (scale * (img_width / window_width))   - (1.0 - (scale * (img_width / window_width)));
    float y = vertexPosition.y * (scale * (img_height / window_height)) + (1.0 - (scale * (img_height / window_height)));

	gl_Position     = vec4(x + (x_pos / window_width), y - (y_pos / window_height), 0.0, 1.0);
	fragTexCoord    = vertexTexCoord;
}