#version 330 core

layout (location = 0) in vec2 vertex_position;
layout (location = 1) in vec2 vertex_texcoord;

uniform float window_width;
uniform float window_height;
uniform float img_width;
uniform float img_height;
uniform float x_pos;
uniform float y_pos;
uniform float scale;

out vec2 frag_tex_coord;

void main()
{
    float x = vertex_position.x * (scale * (img_width / window_width))   - (1.0 - (scale * (img_width / window_width)));
    float y = vertex_position.y * (scale * (img_height / window_height)) + (1.0 - (scale * (img_height / window_height)));
	gl_Position     = vec4(x + (x_pos / window_width), y - (y_pos / window_height), 1.0, 1.0);
	frag_tex_coord    = vertex_texcoord;


    // for quad gradient.
// 	gl_Position     = vec4(vertex_position.x, vertex_position.y, 1.0, 1.0);
// 	frag_tex_coord    = vec2(frag_tex_coord.y, vertex_position.y);
}