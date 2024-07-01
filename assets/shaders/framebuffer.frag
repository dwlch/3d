#version 330 core

in vec2 fragTexCoord;

uniform sampler2D screen_texture;
uniform float gamma;

out vec4 finalColor;

void get_kernel(inout vec4 kernel[9], sampler2D tex, vec2 coord)
{
    float thickness = 1.0;
    float width     = 1000;
    float height    = 800;

	float w = thickness / width;
	float h = thickness / height;

	kernel[0] = texture2D(tex, coord + vec2( -w,    -h));
	kernel[1] = texture2D(tex, coord + vec2(  0.0,  -h));
	kernel[2] = texture2D(tex, coord + vec2(  w,    -h));
	kernel[3] = texture2D(tex, coord + vec2( -w,    0.0));
	kernel[4] = texture2D(tex, coord);
	kernel[5] = texture2D(tex, coord + vec2(  w,    0.0));
	kernel[6] = texture2D(tex, coord + vec2( -w,    h));
	kernel[7] = texture2D(tex, coord + vec2(  0.0,  h));
	kernel[8] = texture2D(tex, coord + vec2(  w,    h));
}

void main(void) 
{
    vec4 kernel[9];
	get_kernel(kernel, screen_texture, fragTexCoord.st);

    float threshold     = 0.1;
	vec4 sobel_edge_h   = kernel[2] + (threshold * kernel[5]) + kernel[8] - (kernel[0] + (threshold * kernel[3]) + kernel[6]);
  	vec4 sobel_edge_v   = kernel[0] + (threshold * kernel[1]) + kernel[2] - (kernel[6] + (threshold * kernel[7]) + kernel[8]);
	vec4 sobel          = sqrt((sobel_edge_h * sobel_edge_h) + (sobel_edge_v * sobel_edge_v));

    // finalColor = texture(screen_texture, fragTexCoord) * vec4(1.0 - sobel.rgb, 1.0);
    finalColor = texture(screen_texture, fragTexCoord);

    vec3 fragment   = texture(screen_texture, fragTexCoord).rgb;
    float exposure  = 1.2;
    vec3 tone_map   = vec3(1.0) - exp(-fragment * exposure);
    //finalColor.rgb  = pow(tone_map, vec3(1.0 / 0.5)) * vec3(1.0 - sobel.rgb);
    finalColor.rgb  = pow(tone_map, vec3(1.0 / 0.5));
}