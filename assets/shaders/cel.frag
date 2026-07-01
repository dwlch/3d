#version 330 core

#define NUM_CASCADES    3
#define NEAR            1.0
#define FAR             1000.0

in vec3 frag_position;                      // fragment position.
in vec3 frag_normal;                        // fragment normal.
in vec3 frag_color;                         // colour of the fragment.
in vec2 frag_texcoord;                      // fragment texture coordinates.
in vec4 frag_shadowcoords[NUM_CASCADES];    // shadowmap coordinates.

uniform sampler2D tex0;                     // material texture.
uniform sampler2D shadow_map[NUM_CASCADES]; // shadow map texture, in 3 different resolutions.
uniform float cascade_bounds[NUM_CASCADES]; // cascade boundaries array.
uniform float camera_distance;              // distance from player to camera.
uniform vec3 albedo;                        // base colour, used for 'ingame' colours.
uniform vec3 light_pos;                     // directional light position.

layout (location = 0) out vec4 final_color;
layout (location = 1) out vec4 bright_color;

// returns which shadowmap cascade the fragment is inside.
int get_cascade(float dist)
{
    // check from closest to second furthest away.
    for (int i = 0; i < NUM_CASCADES - 1; ++i)
    {
        // check distance from current fragment to the camera vs current cascade bound.
        if (dist < cascade_bounds[i])
        {
            return i;
        }
    }

    // return largest cascade in this case.
    return NUM_CASCADES - 1;
}

// bilinear filtered pcf.
float bilinear_pcf(vec3 shadow_coords, int cascade_index)
{
    // convert from (-1, 1) to (0, 1).
    float shadow    = 0.0;
    vec2 frac       = fract(shadow_coords.xy * textureSize(shadow_map[cascade_index], 0) + 0.5);
    vec2 texel_size = 1.0 / textureSize(shadow_map[cascade_index], 0);
    
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            vec2 offset = vec2(x, y) * texel_size;
            vec4 temp   = textureGather(shadow_map[cascade_index], shadow_coords.xy + offset);
            temp.x      = temp.x < shadow_coords.z ? 0.0 : 1.0;
            temp.y      = temp.y < shadow_coords.z ? 0.0 : 1.0;
            temp.z      = temp.z < shadow_coords.z ? 0.0 : 1.0;
            temp.w      = temp.w < shadow_coords.z ? 0.0 : 1.0;
            shadow      += mix( mix(temp.w, temp.z, frac.x), 
                                mix(temp.x, temp.y, frac.x), frac.y);
        }    
    }

    return (shadow / float(pow(2 + 1, 2)));
}

float get_shadow(float intensity, float lol)
{
    float dist          = gl_FragCoord.z / gl_FragCoord.w;
    int current_map     = get_cascade(dist);
    float shadow        = lol;
    vec3 shadow_coords  = ((frag_shadowcoords[current_map].xyz / frag_shadowcoords[current_map].w) + 1.0) / 2.0;

    // early exit.
    if (shadow_coords.z > 1.0)
    {
        return 1.0;
    }

    // float bias = max(0.01 * (1.0 - NdotL), 0.0005);
    // float bias  = 0.05 * tan(acos((NdotL)));
    // bias        = clamp(bias, 0 ,0.01);

    if (texture(shadow_map[current_map], shadow_coords.xy).r < shadow_coords.z)
    {
        shadow = 0.0;
    }

    return clamp(shadow, intensity, 1.0);
}

// get depth fog value.
float get_depth_fog(float steepness, float distance_offset)
{
    float z_val = ((2.0 * NEAR * FAR) / (FAR + NEAR - (gl_FragCoord.z * 2.0 - 1.0) * (FAR - NEAR)));
    return clamp((1.0 / (1.0 + exp(-steepness * (z_val - (camera_distance + distance_offset))))), 0.0, 0.8);
}

void main()
{
    // light properties.
    vec3 light_target   = vec3(0.0);
    vec3 light_dir      = normalize(light_pos - light_target);
    vec3 light_colour   = vec3(1.2);

    // normal dot light.
    float NdotL         = max(dot(frag_normal, light_dir), 0.0);
    float shadow_smooth = 0.0;
    float intensity     = smoothstep(0, shadow_smooth, NdotL);

    // get the emission and diffuse terms.
    vec3 emission       = texture(tex0, frag_texcoord).rgb * albedo;
    vec3 diffuse        = texture(tex0, frag_texcoord).rgb * light_colour;
    vec3 base_color     = emission + diffuse;
    // vec3 y_gradient     = vec3(clamp(frag_position.y, -1.0, 1.0));

    // base_color = y_gradient;

    // base_color          = vec3(base_color.x, base_color.y * frag_normal.y , base_color.z);
    
    // fog calculation using depth buffer.
    // get fog depth according to steepness of gradient and distance offset.
    float fog_depth     = get_depth_fog(0.04, 70.0);
    vec3 fog_color      = fog_depth * vec3(1.0);

    // combine all terms with fog.
    final_color = vec4(base_color * (get_shadow(0.35, intensity)) * (1.0 - fog_depth) + fog_color, 1.0);

    vec3 threshold      = vec3(0.2126, 0.7152, 0.0722);
    // vec3 threshold      = vec3(0.9);
    float brightness = dot(final_color.rgb, threshold);
    if (brightness > 1.0)
    {
        bright_color = vec4(final_color.rgb, 1.0);
    }
    else
    {
        bright_color = vec4(vec3(0.0), 1.0);
    }
}

