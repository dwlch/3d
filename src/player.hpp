#pragma once

#include <vector>
#include <array>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "camera.hpp"
#include "draw.hpp"
#include "collision.hpp"

#define MAX_COLLISION_CHECKS 32
#define COLLIDER_COUNT 2

struct Player
{
    enum State
    {
        GROUND,
        AIR
    };

    State state = State::AIR;
    Model model = Model("player.gltf");
    std::array<CylinderCollider, COLLIDER_COUNT> collider;

    const float HEIGHT          = 4.6f;
    const float RADIUS          = 0.8f;
    const float GROUND_DEPTH    = 0.1f;
    
    glm::vec3 position          = glm::vec3(0.0f);              // player position.
    glm::vec3 camera_lookat     = glm::vec3(0.0f);              // player position.
    glm::vec3 respawn_position  = glm::vec3(0.0f);              // go here on respawn call.
    glm::vec3 scale             = glm::vec3(1.0f);              // scale of player model.
    glm::vec3 colour            = glm::vec3(1.5f);              // ambient colour of player model, stand out while in shadow etc.
    glm::quat model_rotation    = glm::quat(glm::vec3(0.0f));   // rotation quaternion of model.

    // movement physics variables.
    const float ACCEL           = 0.05f;    // movement acceleration.
    const float MAX_SPEED       = 0.15f;    // maxiumum player speed.
    const float TURN_SPEED      = 0.05f;    // speed of model rotation.
    const float JUMP_POWER      = 0.4f;    // jump impulse amount.
    const float GRAVITY         = 0.01f;   // strength of gravity.
    const float MAX_FALL_SPEED  = 1.5f;     // strength of gravity.

    float angle_facing          = 0.0f;     // starts player rotated away from camera.
    float lateral_movement      = 0.0f;     // forward direction speed.
    float vertical_movement     = 0.0f;     // forward direction speed.

    Player();
    void update(std::vector<std::unique_ptr<Collider>> &colliders, Camera &camera);
    void respawn();
    void draw(Shader &mesh_shader, Shader &line_shader);
};
