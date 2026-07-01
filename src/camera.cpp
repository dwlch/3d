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
        25.0f,          // 0 - 40
        100.0f,         // 40 - 300
        300.0f         // 1000 - 1000
    };
}

// callaed first every frame, gets camera input + sets orientation.
void Camera::get_input(double dt)
{
    distance_offset = glm::clamp(distance_offset + ((INPUT_0 - INPUT_1) / 2.0f), DISTANCE_MIN, DISTANCE_MAX);

    // derive yaw and pitch rotations in radians from input axis.
    glm::vec2 input     = glm::vec2(AXIS_1_LEFT - AXIS_1_RIGHT, AXIS_1_UP - AXIS_1_DOWN);
    yaw                 = lerp_float(yaw,   input.x * SENSITIVITY * dt, ACCEL * dt);
    pitch               = lerp_float(pitch, input.y * SENSITIVITY * dt, ACCEL * dt);

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

void Camera::get_cascades()
{
    glm::vec3 light_direction = glm::vec3(glm::normalize(light_pos - light_target));

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
        radius = std::ceil(radius * 16.0f) / 16.0f;

        // snap shadowmap to texels (removes almost all shimmering/flickering on static shadows).
        float texels        = static_cast<float>(SHADOWMAP_SIZE) / (radius *  2.0f);
        glm::mat4 look_at   = glm::lookAt(-light_direction, glm::vec3(0.0f), up) * glm::mat4(texels);
        frustum_centre      = glm::vec3(look_at * glm::vec4(frustum_centre, 1.0f));
        frustum_centre.x    = glm::floor(frustum_centre.x);
        frustum_centre.y    = glm::floor(frustum_centre.y);
        frustum_centre      = glm::vec3(glm::inverse(look_at) * glm::vec4(frustum_centre, 1.0f));

        // get final matrix for cascade.
        glm::vec3 light_target  = frustum_centre - (light_direction * (radius * 2.0f));
        glm::mat4 light_view    = glm::lookAt(frustum_centre, light_target, up);
        glm::mat4 light_proj    = glm::ortho(-radius, radius, -radius, radius, -radius * 2.0f, radius * 2.0f);
        cascade_proj[i]         = light_proj * light_view;
    }
}