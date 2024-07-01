#include "player.hpp"
#include "defines.hpp"
#include "utility.hpp"
#include "input.hpp"
#include <iostream>

using glm::vec2;
using glm::vec3;
using std::cout;

Player::Player()
{
    // we have 2 colliders, one for EPA that is solid, the other that is just for getting grounded state by extending slightly from base of main collider.
    collider[0] = CylinderCollider(position, vec3(0.0f, 1.0f, 0.0f), HEIGHT, RADIUS);
    collider[1] = CylinderCollider(glm::vec3(position.x, position.y - GROUND_DEPTH, position.z), vec3(0.0f, 1.0f, 0.0f), GROUND_DEPTH, RADIUS);
}

void Player::update(std::vector<std::unique_ptr<Collider>> &colliders, Camera &camera)
{
    // derive input from input axis 0.
    vec2 input = vec2(AXIS_0_RIGHT - AXIS_0_LEFT, AXIS_0_DOWN - AXIS_0_UP);

    // player state handler.
    switch (state)
    {
        case State::GROUND:
            vertical_movement   = JUMP_POWER * SPACE_PRESSED;
            collider[1].colour  = glm::vec3(0.9f, 0.4f, 0.3f);
            break;
        
        case State::AIR:
            collider[1].colour  = glm::vec3(0.0f);
            vertical_movement   = glm::clamp((vertical_movement - GRAVITY), -MAX_FALL_SPEED, JUMP_POWER);
            break;
    }

    // only move if input is non-zero.
    if (!is_vec2_zero(input))
    {
        input           = glm::normalize(input);                                    // normalise input vector (1).
        vec3 forward    = glm::normalize(camera.get_position(position) - position); // get forward vector by subtracting the player position from the camera position.
        float rot       = atan2f(input.x, input.y) + atan2f(forward.x, forward.z);  // get rotation angle from input and forward.
        angle_facing    = atan2(sin(rot), cos(rot));                                // wrap rotation from -pi to pi.   
    }

    // calculate total movement for next frame.
    lateral_movement        = lerp_float(lateral_movement, MAX_SPEED * glm::length(input), ACCEL);
    collider[0].position    = position + rotate(vec3(0.0f, vertical_movement, lateral_movement), angle_facing, camera.up);
    model_rotation          = glm::slerp(model_rotation, glm::quat(vec3(0.0f, angle_facing, 0.0f)), TURN_SPEED);

    for (int i = 0; i < MAX_COLLISION_CHECKS; i++)
    {
        int count = 0;
        for (size_t i = 0; i < colliders.size(); i++)
        {
            Results collision = is_collision(&collider[0], &*colliders[i]);
            if (collision.collided)
            {
                count++;
                collider[0].position -= (collision.normal * collision.depth);
            }
        }

        // after every round of collision checks, check if there were none.
        if (count == 0)
        {
            break;
        }
    }

    collider[1].position    = glm::vec3(collider[0].position.x, collider[0].position.y - collider[1].height, collider[0].position.z);
    state                   = State::AIR;

    for (size_t i = 0; i < colliders.size(); i++)
    {
        Results collision = is_collision(&collider[1], &*colliders[i]);
        if (collision.collided)
        {
            state = State::GROUND;
        }
    }
    // out of bounds collision.
    if ((collider[0].position.y < -100.0f) || (INPUT_3 == 1.0f))
    {
        std::cout << "respawn" << "\n";
        respawn();
    }

    // after collision checks, set new positon to the resolved collider position.
    position        = collider[0].position;
    camera_lookat   = vec3(position.x, position.y + HEIGHT, position.z);
}

// draw player.
void Player::draw(Shader &mesh_shader, Shader &line_shader)
{
    model.draw(position, model_rotation, scale, mesh_shader, colour);

    // option to draw colliders.
    // for (size_t i = 0; i < collider.size(); i++)
    // {
    //     collider[i].draw(line_shader);
    // }
}

// reset player position.
// function cause might want to call from other files mb?? unsure.
void Player::respawn()
{
    collider[0].position = respawn_position;
}