#pragma once

#include <vector>
#include <array>
#include <glad.h>
#include "glm/glm.hpp"                      // maths lib.
#include "glm/gtx/rotate_vector.hpp"        // glm::rotate().
#include "glm/gtx/vector_angle.hpp"         // glm::orientedAngle().

#include "defines.hpp"

struct CameraRail
{

};


struct Camera
{
    glm::mat4 projection        = glm::mat4(1.0f);
    glm::mat4 view              = glm::mat4(1.0f);
    glm::mat4 mvp               = glm::mat4(1.0f);              // view matrix that is sent to shader.

    glm::vec3 target_pos        = glm::vec3(0.0f);              // store postion of target for shader effects (fog).
    glm::vec3 up                = glm::vec3(0.0f, 1.0f, 0.0f);  // camera up axis.
    glm::vec3 orientation       = glm::vec3(0.0f, 0.0f, 1.0f);  // stores the camera's relative position without distance.
    glm::vec3 light_pos         = glm::vec3(0.5f, 1.0f, 0.5f);
    glm::vec3 light_target      = glm::vec3(0.0f);

    // constants.
    const float NEAR_PLANE      = 0.1f;     // how close can render to camera.
    const float FAR_PLANE       = 1000.0f;  // how far can render to camera.
    const float ANGLE_MIN       = 0.6f;     // minimum vertical angle.
    const float ANGLE_MAX       = 3.1f;     // maximum vertical angle.
    const float FOV_MIN         = 75.0f;    // minimum FOV value.
    const float FOV_MAX         = 90.0f;   // maximum FOV value.
    const float FOV_SCALE       = 8.0f;     // dynamic FOV multiplier. larger = greater change.
    const float DISTANCE_MIN    = 1.0f;     // minimum distance offset value.
    const float DISTANCE_MAX    = 100.0f;   // maximum distance offset value.
    const float DISTANCE_SCALE  = 0.4f;     // dynamic distance multiplier. larger = greater change.
    const float ACCEL           = 100.0f;   // camera movement speed multiplier.
    const float SENSITIVITY     = 500.0f;

    // variables.
    float pitch                 = 0.0f;
    float yaw                   = 0.0f;
    float FOV;                      // initial field of view value.
    float aspect;                   // aspect ratio.
    float distance_offset = 7.0f;   // distance from camera to target.

    // shadowmap.
    GLuint FBO;
    std::array<GLuint, NUM_CASCADES> depth_maps;
    std::array<glm::mat4, NUM_CASCADES> cascade_proj;
    std::array<float, NUM_CASCADES> cascade_bounds;

    // functions.
    Camera();
    void get_input(float auto_cam, float dt);                           // get input + calc orientation using input.
    glm::vec3 get_position(glm::vec3 target);   // return camera position relative to target.
    void update(glm::vec3 target);              // update the camera view matrix.
    void get_cascades(glm::vec3 light_position);
};


struct Plane
{
    glm::vec3 normal    = glm::vec3(0.0f, 1.0f, 0.0f);
    float distance      = 0.0f;

    Plane() {};
    Plane(const glm::vec3 &p1, const glm::vec3 &norm) : normal(glm::normalize(norm)), distance(glm::dot(normal, p1)) {}

    void normalize()
    {
        float length    = glm::length(normal);
        normal          /= length;
        distance        /= length;
    }

};

struct Frustum
{
    Plane top;
    Plane bottom;

    Plane right;
    Plane left;

    Plane far_;
    Plane near_;

    std::array<Plane, 6> planes;

    Frustum(const glm::mat4& mvp);
    bool is_inside(const glm::vec3& min, const glm::vec3& max);
};