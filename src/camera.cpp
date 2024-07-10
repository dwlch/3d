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