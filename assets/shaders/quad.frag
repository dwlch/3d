#version 330 core

in vec2 frag_tex_coord;
uniform sampler2D tex0;
layout (location = 0) out vec4 final_color;

void main(void) 
{
    final_color = texture(tex0, frag_tex_coord);
    // vec3 col_a = vec3(1.5, 1.6, 1.7);
    // vec3 col_b = vec3(1.1, 1.9, 1.2);


    // finalColor = vec4(vec3(col_a * frag_tex_coord.x) + vec3(col_b * frag_tex_coord.y), 1.0);
}