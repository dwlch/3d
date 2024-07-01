#include "camera.hpp"
#include "input.hpp"
#include "defines.hpp"
#include "utility.hpp"  // used for lerp_float on input.
#include <iostream>

#include <vector>
#include <array>

Camera::Camera()
{

}

// callaed first every frame, gets camera input + sets orientation.
void Camera::get_input()
{
    distance_offset = glm::clamp(distance_offset + ((INPUT_0 - INPUT_1) / 2.0f), DISTANCE_MIN, DISTANCE_MAX);

    // derive yaw and pitch rotations in radians from input axis.
    glm::vec2 input     = glm::vec2(AXIS_1_LEFT - AXIS_1_RIGHT, AXIS_1_UP - AXIS_1_DOWN);
    yaw                 = lerp_float(yaw,   (input.x * SENSITIVITY), ACCEL);
    pitch               = lerp_float(pitch, (input.y * SENSITIVITY), ACCEL);

    // constrain to min and max angle.
    // if new angle from pitch goes outside bounds, set pitch to 0.
    float angle = glm::orientedAngle(up, glm::rotate(orientation, pitch, glm::cross(orientation, up)), up);
    if (angle < ANGLE_MIN || angle > ANGLE_MAX)
    {
        pitch = 0.0f;
    }

    // rotate the camera using yaw and pitch.
    orientation = glm::rotate(orientation, yaw,     up);
    orientation = glm::rotate(orientation, pitch,   glm::cross(orientation, up));
    orientation = glm::normalize(orientation);
}

// derive camera position relative to target.
glm::vec3 Camera::get_position(glm::vec3 target)
{
    target_pos      = target;
    float distance  = glm::orientedAngle(up, orientation, up) * distance_offset;
    target.y        = target.y + (distance * DISTANCE_SCALE);
    FOV             = glm::clamp(FOV_BASE - (distance * FOV_SCALE), FOV_MIN, FOV_MAX);

    // return camera position relative to target.
    return (target - (orientation * distance));
}

// update camera position according to orientation got from input function + target position.
void Camera::update(glm::vec3 target)
{
    aspect      = (float)(WINDOW_WIDTH)/(float)(WINDOW_HEIGHT);
    projection  = glm::perspective(glm::radians(FOV), aspect, NEAR_PLANE, FAR_PLANE);
    view        = glm::lookAt(get_position(target), target, up);
	mvp         = projection * view;
}

// this version works but is not correct yet. keep it for now while working on other version.
// void Camera::get_light_projection(std::array<glm::mat4, 4> &cascade_proj, glm::vec3 &light_direction)
// {
//     std::array<float, 4> cascade {
//         NEAR_PLANE, 25.0f, 100.0f, 400.0f
//     };

//     int NUM_CASCADES = 3;

//     // loop through each cascade.
//     for (int i = 0; i < NUM_CASCADES; i++)
//     {
//         // get inverse of the camera view (at each cascades near and far bounds).
//         glm::mat4 proj  = glm::perspective(glm::radians(FOV), aspect, cascade[i], cascade[i + 1]);
//         glm::mat4 inv   = glm::inverse(proj * view);
        
//         // get 8 corners of a frustum (method from opengl tutorial site)
//         std::vector<glm::vec4> corners;
//         for (unsigned int x = 0; x < 2; x++)
//         {
//             for (unsigned int y = 0; y < 2; y++)
//             {
//                 for (unsigned int z = 0; z < 2; z++)
//                 {
//                     glm::vec4 pt = inv * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
//                     corners.push_back(pt / pt.w);
//                 }
//             }
//         }

//         // get average of all corners as target / centre of frustum.
//         glm::vec3 frustum_centre = glm::vec3(0.0f);
//         for (int j = 0; j < 8; j++)
//         {
//             frustum_centre += glm::vec3(corners[j]);
//         }
//         frustum_centre /= corners.size();

//         // get light view matrix respetive to light direction and centre of fustrum.
//         glm::mat4 light_view = glm::lookAt(frustum_centre + light_direction, frustum_centre, up);
        
//         // min and max bounds.
//         glm::vec3 ortho_min(std::numeric_limits<float>::max());
//         glm::vec3 ortho_max(std::numeric_limits<float>::lowest());
//         for (size_t j = 0; j < 8; j++)
//         {
//             corners[j] = light_view * corners[j];

//             ortho_min.x = std::min(ortho_min.x, corners[j].x);
//             ortho_max.x = std::max(ortho_max.x, corners[j].x);
//             ortho_min.y = std::min(ortho_min.y, corners[j].y);
//             ortho_max.y = std::max(ortho_max.y, corners[j].y);
//             ortho_min.z = std::min(ortho_min.z, corners[j].z);
//             ortho_max.z = std::max(ortho_max.z, corners[j].z);
//         }

//         // output the light projection.
//         glm::mat4 light_proj    = glm::ortho(ortho_min.x, ortho_max.x, ortho_min.y, ortho_max.y, -ortho_max.z, -ortho_min.z);
//         cascade_proj[i]         = light_proj * light_view;
//     }
// }

glm::vec3 transform_by_matrix(const glm::vec3 &point, const glm::mat4 &matrix)
{
    glm::vec4 temp = matrix * glm::vec4(point, 1.0f);
    return glm::vec3(temp);
}

// version 2.
void Camera::get_light_projection(std::array<glm::mat4, 3> &cascade_proj, glm::vec3 &light_direction)
{
    std::array<float, 4> cascade {
        NEAR_PLANE, 25.0f, 100.0f, 400.0f
    };

    for (int index = 0; index < NUM_CASCADES; index++)
    {
        // get inverse of the camera view (at each cascades near and far bounds).
        glm::mat4 proj  = glm::perspective(glm::radians(FOV), aspect, cascade[index], cascade[index + 1]);
        glm::mat4 inv   = glm::inverse(proj * view);
        
        // get 8 corners of the frustum in world space.
        std::vector<glm::vec3> corners = {
            glm::vec3(-1.0f,  1.0f, 0.0f),
            glm::vec3( 1.0f,  1.0f, 0.0f),
            glm::vec3( 1.0f, -1.0f, 0.0f),
            glm::vec3(-1.0f, -1.0f, 0.0f),
            glm::vec3(-1.0f,  1.0f, 1.0f),
            glm::vec3( 1.0f,  1.0f, 1.0f),
            glm::vec3( 1.0f, -1.0f, 1.0f),
            glm::vec3(-1.0f, -1.0f, 1.0f),
        };

        // transform corners into light space(?). i think.
        for( int j = 0; j < 8; j++)
        {
            glm::vec4 temp  = inv * glm::vec4(corners[j], 1.0f);
            corners[j]      = glm::vec3(temp / temp.w);
        }

        // get average of all corners as target / centre of frustum.
        glm::vec3 frustum_centre = glm::vec3(0.0f);
        for (int j = 0; j < 8; j++)
        {
            frustum_centre += corners[j];
        }
        frustum_centre *= (1.0f / 8.0f);

        // get the radius of a bounding sphere surrounding the frustum corners.
        float radius = 0.0f;
        for(int j = 0; j < 8; j++)
        {
            float dist  = glm::length(corners[j] - frustum_centre);
            radius      = std::max(radius, dist);
        }
        radius = std::ceil(radius * 16.0f) / 16.0f;

        float texels        = 2048.0f / (radius * 2.0f);
        glm::mat4 look_at   = glm::lookAt(-light_direction, glm::vec3(0.0f), up) * glm::mat4(texels);
        frustum_centre      = transform_by_matrix(frustum_centre, look_at);
        frustum_centre.x    = glm::floor(frustum_centre.x);
        frustum_centre.y    = glm::floor(frustum_centre.y);
        frustum_centre      = transform_by_matrix(frustum_centre, glm::inverse(look_at));

        // get final matrix for cascade.
        glm::vec3 light_target  = frustum_centre - (light_direction * (radius * 2.0f));
        glm::mat4 light_view    = glm::lookAt(frustum_centre, light_target, up);
        glm::mat4 light_proj    = glm::ortho(-radius, radius, -radius, radius, -radius * 6.0f, radius * 6.0f);
        cascade_proj[index]     = light_proj * light_view;
    }
}