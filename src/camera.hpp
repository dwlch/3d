#pragma once

#include <vector>
#include <array>
#include <glad.h>
#include "glm/glm.hpp"                      // maths lib.
#include "glm/gtx/rotate_vector.hpp"        // glm::rotate().
#include "glm/gtx/vector_angle.hpp"         // glm::orientedAngle().

struct Camera
{
    glm::mat4 projection        = glm::mat4(1.0f);
    glm::mat4 view              = glm::mat4(1.0f);
    glm::mat4 mvp               = glm::mat4(1.0f);              // view matrix that is sent to shader.
    glm::vec3 target_pos        = glm::vec3(0.0f);              // store postion of target for shader effects (fog).
    glm::vec3 up                = glm::vec3(0.0f, 1.0f, 0.0f);  // camera up axis.
    glm::vec3 orientation       = glm::vec3(0.0f, 0.0f, 1.0f);  // stores the camera's relative position without distance.


    // shadowmap.
    // std::array<GLuint, 3> depth_maps;
    // std::array<glm::mat4, 3> cascade_proj;
    // std::array<float, 4> cascade_bounds {
    //     1.0f, 20.0f, 80.0f, 400.0f
    // };

    // int size;
    // GLuint FBO;

    // ShadowMap(int size);
    // glm::mat4 get_cascades(Camera &camera, glm::vec3 &light_direction);
    // void update_uniforms(Shader &shader, Camera &camera, glm::vec3 &light_pos);




    // constants.
    const float NEAR_PLANE      = 1.0f;         // how close can render to camera.
    const float FAR_PLANE       = 1000.0f;      // how far can render to camera.
    const float ANGLE_MIN       = 1.0f;         // maximum vertical angle.
    const float ANGLE_MAX       = 3.1f;         // maximum vertical angle.

    const float FOV_BASE        = 75.0f;        // initial field of view value.
    const float FOV_MIN         = 35.0f;        // minimum FOV value.
    const float FOV_MAX         = 100.0f;       // maximum FOV value.

    const float DISTANCE_MIN    = 10.0f;        // minimum distance offset value.
    const float DISTANCE_MAX    = 100.0f;        // maximum distance offset value.

    const float FOV_SCALE       = 0.0f;         // dynamic FOV multiplier. larger = greater change.
    const float DISTANCE_SCALE  = 0.3f;         // dynamic distance multiplier. larger = greater change.
    const float ACCEL           = 0.1f;         // camera movement speed multiplier.
    const float SENSITIVITY     = 0.01f;        // camera movement speed multiplier.

    // variables.
    float FOV                   = FOV_BASE;     // initial field of view value.
    float aspect                = 0.0f;         // aspect ratio.
    float yaw                   = 0.0f;         // horizontal rotation angle in radians.
    float pitch                 = 0.0f;         // vertical rotation angle in radians.
    float distance_offset       = 10.0f;        // distance from camera to target.

    // functions.
    glm::vec3 get_position(glm::vec3 target);   // return camera position relative to target.
    Camera();
    void get_input();                           // get input + calc orientation using input.
    void update(glm::vec3 target);              // update the camera view matrix.
};