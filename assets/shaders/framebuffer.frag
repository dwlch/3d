#version 330 core

in vec2 fragTexCoord;
uniform sampler2D screen_texture;
uniform sampler2D bloom;

uniform float gamma;

out vec4 finalColor;

void get_kernel(inout vec4 kernel[9], sampler2D tex, vec2 coord)
{
    float thickness = 0.5;
    float width     = 1100;
    float height    = 900;

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

    float threshold     = 0.001;
	vec4 sobel_edge_h   = kernel[2] + (threshold * kernel[5]) + kernel[8] - (kernel[0] + (threshold * kernel[3]) + kernel[6]);
  	vec4 sobel_edge_v   = kernel[0] + (threshold * kernel[1]) + kernel[2] - (kernel[6] + (threshold * kernel[7]) + kernel[8]);
	vec4 sobel          = sqrt((sobel_edge_h * sobel_edge_h) + (sobel_edge_v * sobel_edge_v));

    // finalColor = texture(screen_texture, fragTexCoord) * vec4(1.0 - sobel.rgb, 1.0);
    // finalColor  = texture(screen_texture, fragTexCoord);

    finalColor  = texture(screen_texture, fragTexCoord);
    // finalColor  += (texture(bloom, fragTexCoord) * 1.0);
    // finalColor = vec4(clamp(finalColor.r, 0.0, 1.0), clamp(finalColor.g, 0.0, 1.0), clamp(finalColor.b, 0.0, 1.0), 1.0);

    // finalColor  = texture(bloom, fragTexCoord);


    // finalColor += bloomColor;

    
    // finalColor  = texture(bloom, fragTexCoord);

    float exposure  = 1.2;
    vec3 tone_map   = vec3(1.0) - exp(-finalColor.rgb * exposure);

    float g         = 0.6;
    // vec3 result     = vec3(1.0) - exp(-finalColor.rgb * exposure);
    // result          = pow(result, vec3(1.0 / g));

    
    // finalColor.rgb  = pow(tone_map, vec3(1.0 / g)) * vec3(1.0 - vec3(sobel.r + sobel.g + sobel.b));
    finalColor.rgb  = pow(tone_map, vec3(1.0 / g));

    // final_color.rgb = result;

}