#pragma once

#include <vector>
#include <array>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "camera.hpp"
#include "draw.hpp"
#include "collision.hpp"
#include "level.hpp"

#define MAX_COLLISION_CHECKS    32
#define COLLIDER_COUNT          3

#define ANIMATION_IDLE  0
#define ANIMATION_RUN   1
#define ANIMATION_JUMP  2

#define COLLIDER_MAIN   0
#define COLLIDER_GROUND 1
#define COLLIDER_SEARCH 2

struct Player
{
    enum State
    {
        GROUND,
        AIR
    };

    State state = State::AIR;
    Model model;
    std::array<CylinderCollider, COLLIDER_COUNT> collider;
    

    const float HEIGHT          = 3.0f;
    const float RADIUS          = 0.8f;
    const float GROUND_DEPTH    = 0.1f;
    const float RAY_DEPTH       = 1.0f;
    
    glm::vec3 position          = glm::vec3(0.0f);              // player position.
    glm::vec3 forward           = glm::vec3(0.0f, 0.0f, 1.0f);  // player position.
    glm::vec3 up                = glm::vec3(0.0f, 1.0f, 0.0f);  // player upward direction.
    glm::vec3 camera_lookat     = glm::vec3(0.0f);              // player position.
    glm::vec3 respawn_position  = glm::vec3(0.0f);              // go here on respawn call and level update.
    glm::vec3 scale             = glm::vec3(1.0f);              // scale of player model.
    glm::vec3 colour            = glm::vec3(1.5f);              // ambient colour of player model, stand out while in shadow etc.
    glm::quat model_rotation    = glm::quat(glm::vec3(0.0f));   // rotation quaternion of model.

    Line line{glm::vec3(0.0f, 1.0f, 0.0f), 5.0f};
    glm::quat line_rotation     = glm::quat(glm::vec3(0.0f));

    int current_level           = 0;


    int steps_since_grounded    = 0;
    uint32_t target_animation   = 0;

    // movement physics variables.
    const float ACCEL           = 7.5f;    // movement acceleration.
    const float MAX_SPEED       = 12.5f;    // maxiumum player speed.

    const float ANIM_SPEED      = 1.2f;     // animation speed.
    const float TURN_SPEED      = 10.0f;     // speed of model rotation.

    const float JUMP_POWER      = 750.0f;    // jump impulse amount.
    const float GRAVITY         = -90.0f;     // strength of gravity.
    const float MAX_FALL_SPEED  = 45.0f;   // fall speed clamp.

    const float GROUND_MIN      = 1.9f;     // minimum angle in radians for ground flag.
    


    float angle_facing          = 0.0f;     // starts player rotated away from camera.
    float movement_h            = 0.0f;     // forward direction speed.
    float movement_v            = 0.0f;     // upward direction speed.
    float animation_speed       = 0.0f;
    float target_v              = 0.0f;

    float coyote_time           = 0.15f;
    float coyote_time_count     = 0.0f;

    float slope_force           = 90.0f;
    float slope_force_depth     = 0.0f;

    float jump_hold             = 0.2f;
    float jump_buffer           = 0.001f;
    float jump_buffer_count     = 0.0f;
    float jump_height           = 50.0f;
    float velocity_y            = 0.0f;

    bool jumping                = true;
    bool grounded               = false;

    Player(std::string model_name);
    void update(double dt, Level &level, Camera &camera);
    void move(glm::vec3 movement, Level &level);
    void respawn(Level &level);
    void jump();
    glm::vec3 get_slope(std::vector<std::unique_ptr<Collider>> &colliders);
    void draw(Shader mesh_shader, Shader line_shader, Camera camera, bool draw_collider);
};
