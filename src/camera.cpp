#include "camera.hpp"
#include "input.hpp"
#include "defines.hpp"
#include "utility.hpp"  // used for lerp_float on input.
#include <iostream>

#include <vector>
#include <array>

Camera::Camera()
{
    // Depth texture. Slower than a depth buffer, but you can sample it later in your shader.
    glGenFramebuffers(1, &FBO);
    glGenTextures(3, depth_maps.data());

    for (unsigned int i = 0; i < depth_maps.size(); ++i)
    {
        glBindTexture(GL_TEXTURE_2D, depth_maps[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, SHADOWMAP_SIZE, SHADOWMAP_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        float border_colour[]  = {1.0f, 1.0f, 1.0f, 1.0f};
	    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_colour);
    }

    // create framebuffer and texture for shadowmap.
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_maps[0], 0);

    // disable writing to colour buffer.
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    
    // check if any errors with framebuffer.
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "FRAMEBUFFER NOT COMPLETE!" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // cascade_bounds[0] = 30.0f;      
    // cascade_bounds[1] = 500.0f;
    // cascade_bounds[2] = 1000.0f;
    cascade_bounds = {
        25.0f,  // 0 - 25
        100.0f, // 25 - 300
        300.0f  // 300 - 1000
    };
}

float timing            = 0.0f;
float auto_cam_factor   = 0.1f;

// called first every frame, gets camera input + sets orientation.
void Camera::get_input(float auto_cam, float dt)
{
    // timing += dt;
    // derive yaw and pitch rotations in radians from input axis.
    float yaw           = (float)MOUSE_X * SENSITIVITY * dt;
    float pitch         = (float)MOUSE_Y * SENSITIVITY * dt;
    distance_offset     = glm::clamp(distance_offset - (float)CAMERA_ZOOM, DISTANCE_MIN, DISTANCE_MAX);

    // clamp pitch to min and max angle.
    float current_angle = glm::orientedAngle(up, orientation, up);
    pitch               = glm::clamp(pitch, current_angle - ANGLE_MAX, current_angle - ANGLE_MIN);

    
    // if (CAMERA_INPUT)
    // {
    //     auto_cam_factor = 0.0f;
    //     timing          = 0.0f;
    // } else if (timing > 1.0f)
    // {
    //     auto_cam_factor = 0.1f;
    // }

    if (CAMERA_INPUT)
    {
        auto_cam_factor = 0.0f;
    }
    else
    {
        auto_cam_factor = 0.1f;
    }

    // rotate the camera using yaw and pitch.
    orientation = glm::rotate(orientation, yaw + (auto_cam * auto_cam_factor), up);
    orientation = glm::rotate(orientation, pitch, glm::cross(orientation, up));
    orientation = glm::normalize(orientation);
}

// derive camera position relative to target.
glm::vec3 Camera::get_position(glm::vec3 target)
{
    float current_angle = glm::orientedAngle(up, orientation, up);
    float distance      = current_angle * distance_offset;
    target_pos          = target;
    target.y            = target.y + (distance * DISTANCE_SCALE);

    // dynamic FOV relative to camera height.
    // could make this a bit smoother/more controlled.
    // ideally want like, an upper and lower limit that the FOV moves through mapped to the camera height,
    // this is sort of it but not quite robust enough yet.
    // probably need to utlise ANGLE_MIN and ANGLE_MAX?
    // maybe mod the angle with min/max?
    // FOV                 = glm::clamp(FOV_MAX - (std::fmod(current_angle - ANGLE_MIN, current_angle) * 10.0f), FOV_MIN, FOV_MAX);


    FOV = FOV_MAX - (std::fmod(current_angle - ANGLE_MIN, current_angle) * FOV_SCALE);
    // std::cout << FOV << "\n";

    // return camera position relative to target.
    return target - (orientation * distance);
}

// update camera position according to orientation got from input function + target position.
void Camera::update(glm::vec3 target)
{
    target_pos  = target;
    aspect      = (float)(WINDOW_WIDTH)/(float)(WINDOW_HEIGHT);
    projection  = glm::perspective(glm::radians(FOV), aspect, NEAR_PLANE, FAR_PLANE);
    view        = glm::lookAt(get_position(target), target, up);
	mvp         = projection * view;
}

void Camera::get_cascades(glm::vec3 light_position)
{
    light_pos                   = light_position;
    glm::vec3 light_direction   = glm::vec3(glm::normalize(light_pos - light_target));

    std::array<float, NUM_CASCADES + 1> cascade {
        NEAR_PLANE, cascade_bounds[0], cascade_bounds[1], cascade_bounds[2]
    };
    
    for (int i = 0; i < NUM_CASCADES; ++i)
    {
        // get inverse of the camera view (at each cascades near and far bounds).
        glm::mat4 proj  = glm::perspective(glm::radians(FOV), aspect, cascade[i], cascade[i + 1]);
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
        glm::vec3 frustum_centre = glm::vec3(0.0f);
        for (size_t j = 0; j < corners.size(); ++j)
        {
            glm::vec4 temp  = inv * glm::vec4(corners[j], 1.0f);
            corners[j]      = glm::vec3(temp / temp.w);
            frustum_centre += corners[j];
        }

        // get average of all corners as target / centre of frustum.
        frustum_centre *= (1.0f / corners.size());

        // get the radius of a bounding sphere surrounding the frustum corners.
        float radius = 0.0f;
        for (size_t j = 0; j < corners.size(); ++j)
        {
            float dist  = glm::length(corners[j] - frustum_centre);
            radius      = std::max(radius, dist);
        }
        // why 16? 8 corners * 2?
        radius = std::ceil(radius * 16.0f) / 16.0f;

        // snap shadowmap to texels (removes almost all shimmering/flickering on static shadows).
        float texels            = (float)(SHADOWMAP_SIZE) / (radius *  2.0f);
        glm::mat4 look_at       = glm::lookAt(-light_direction, glm::vec3(0.0f), up) * glm::mat4(texels);
        frustum_centre          = glm::vec3(look_at * glm::vec4(frustum_centre, 1.0f));
        frustum_centre.x        = glm::floor(frustum_centre.x);
        frustum_centre.y        = glm::floor(frustum_centre.y);
        frustum_centre          = glm::vec3(glm::inverse(look_at) * glm::vec4(frustum_centre, 1.0f));

        // get final matrix for cascade.
        glm::vec3 light_target  = frustum_centre - (light_direction * (radius * 2.0f));
        glm::mat4 light_view    = glm::lookAt(frustum_centre, light_target, up);
        glm::mat4 light_proj    = glm::ortho(-radius, radius, -radius, radius, -radius * 2.0f, radius * 2.0f);
        cascade_proj[i]         = light_proj * light_view;
    }
}

// returns true if given bounding box (min and max) is inside frustum.
bool Frustum::is_inside(const glm::vec3& min, const glm::vec3& max)
{
    for (int i = 0; i < 6; ++i)
    {
        const glm::vec3& normal = planes[i].normal;

        glm::vec3 p_vertex = min;

        if (normal.x >= 0.0f) p_vertex.x = max.x;
        if (normal.y >= 0.0f) p_vertex.y = max.y;
        if (normal.z >= 0.0f) p_vertex.z = max.z;

        // Test only the p-vertex against the plane
        float distance = glm::dot(normal, p_vertex) + planes[i].distance;

        // If the vertex furthest in the direction of the normal is behind the plane,
        // the entire box is definitively behind the plane.
        if (distance < 0.0f) 
        {
            return false;
        }
    }
    return true;
}

// https://www.cosmiclearn.com/opengl/culling.php
Frustum::Frustum(const glm::mat4& mvp)
{
    planes[0].normal.x = mvp[0][3] + mvp[0][0];
    planes[0].normal.y = mvp[1][3] + mvp[1][0];
    planes[0].normal.z = mvp[2][3] + mvp[2][0];
    planes[0].distance = mvp[3][3] + mvp[3][0];

    // Right Plane: row 3 - row 0
    planes[1].normal.x = mvp[0][3] - mvp[0][0];
    planes[1].normal.y = mvp[1][3] - mvp[1][0];
    planes[1].normal.z = mvp[2][3] - mvp[2][0];
    planes[1].distance = mvp[3][3] - mvp[3][0];

    // Bottom Plane: row 3 + row 1
    planes[2].normal.x = mvp[0][3] + mvp[0][1];
    planes[2].normal.y = mvp[1][3] + mvp[1][1];
    planes[2].normal.z = mvp[2][3] + mvp[2][1];
    planes[2].distance = mvp[3][3] + mvp[3][1];

    // Top Plane: row 3 - row 1
    planes[3].normal.x = mvp[0][3] - mvp[0][1];
    planes[3].normal.y = mvp[1][3] - mvp[1][1];
    planes[3].normal.z = mvp[2][3] - mvp[2][1];
    planes[3].distance = mvp[3][3] - mvp[3][1];

    // Near Plane: row 3 + row 2
    planes[4].normal.x = mvp[0][3] + mvp[0][2];
    planes[4].normal.y = mvp[1][3] + mvp[1][2];
    planes[4].normal.z = mvp[2][3] + mvp[2][2];
    planes[4].distance = mvp[3][3] + mvp[3][2];

    // Far Plane: row 3 - row 2
    planes[5].normal.x = mvp[0][3] - mvp[0][2];
    planes[5].normal.y = mvp[1][3] - mvp[1][2];
    planes[5].normal.z = mvp[2][3] - mvp[2][2];
    planes[5].distance = mvp[3][3] - mvp[3][2];

    // Normalize all planes for accurate distance calculations
    for (size_t i = 0; i < planes.size(); ++i)
    {
        planes[i].normalize();
    }
}