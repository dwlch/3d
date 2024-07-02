#version 330 core

#define NUM_CASCADES 3

// input vertex attributes (from vertex shader)
in vec3 fragPosition;   // fragment position.
in vec3 fragNormal;     // fragment normal.
in vec3 fragColor;      // colour of the fragment.
in vec2 fragTexCoord;   // fragment texture coordinates.
in vec4 fragPosLight[NUM_CASCADES];   // positions from from light perspective.
in float ClipSpacePosZ;


uniform sampler2D tex0;                         // material texture.
uniform sampler2D shadow_map[NUM_CASCADES];     // shadow map texture, in 3 different resolutions.
uniform float camera_distance;
uniform float cascade_bounds[3];

uniform vec3 albedo;                    // base colour.
uniform vec3 light_pos;                 // directional light position.

out vec4 final_color;            // output fragment color.

#define NEAR    1.0
#define FAR     1000.0

int chooseCascade()
{
    float test[3];
    test[0] = 25.0;
    test[1] = 100.0;
    test[2] = 400.0;

    // get distance from current fragment to the camera.
    float d = gl_FragCoord.z / gl_FragCoord.w;
    for (int i = 0; i < NUM_CASCADES - 1; i++)
    {
        if (d < test[i])
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
    
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
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

float pcss(float intensity)
{
    float shadow = 0.0;
    int current_map     = chooseCascade();
    vec3 shadow_coords  = ((fragPosLight[current_map].xyz / fragPosLight[current_map].w) + 1.0) / 2.0;

    // // early exit.
    // if (shadow_coords.z > 1.0)
    // {
    //     return 1.0;
    // }

    shadow = bilinear_pcf(shadow_coords, current_map);
    
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
    vec3 light_col      = vec3(1.2);

    // normal dot light.
    float NdotL         = max(dot(fragNormal, light_dir), 0.0);
    float shadow_smooth = 0.4;
    float intensity     = smoothstep(0, shadow_smooth, NdotL);

    // get the ambient and diffuse terms.
    vec3 emission       = texture(tex0, fragTexCoord).rgb * albedo;
    vec3 diffuse        = texture(tex0, fragTexCoord).rgb * intensity * light_col;
    
    // fog calculation using depth buffer.
    // get fog depth according to steepness of gradient and distance offset.
    float fog_depth     = get_depth_fog(0.04, 150.0);
    vec3 fog_color      = fog_depth * vec3(1.5);

    // combine all terms: emission + diffuse, shadows, then fog.
    // uint current_map = chooseCascade(fragPos, cameraPos);
    final_color = vec4((emission + diffuse) * (pcss(0.3)) * (1.0 - fog_depth) + fog_color, 1.0);
}

