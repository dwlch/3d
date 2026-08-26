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
    // load player model according to given gltf filename.
    model.load_from_file("player_test.glb");

    // 2 colliders: one for collision, the other that is just for getting grounded state by extending slightly from base of main collider.
    collider[COLLIDER_MAIN]     = CylinderCollider(position,                        up, HEIGHT,         RADIUS);
    collider[COLLIDER_GROUND]   = CylinderCollider(position - (up * GROUND_DEPTH),  up, GROUND_DEPTH,   RADIUS);

    // setup animation looping.
    if (model.animations.size() > ANIMATION_IDLE)
    {
        model.animations[ANIMATION_IDLE].loop = true;
    }
    if (model.animations.size() > ANIMATION_RUN)
    {
        model.animations[ANIMATION_RUN].loop = true;
    }
    if (model.animations.size() > ANIMATION_JUMP)
    {
        model.animations[ANIMATION_JUMP].loop = false;
    }

    // set level on load.
    current_level   = 1;
    sound           = Sound("jump_1", SAMPLE_RATE, false, 0.3f);
}

float last_x = 0.0f;

void Player::update(std::unique_ptr<Level> &level, Skybox &skybox, Sound &bgm, Camera &camera, float dt)
{
    glm::vec3 prev      = position;
    vec2 input          = vec2(HORIZONTAL, VERTICAL);
    vec3 slope_angle    = get_slope(level->colliders);

    if (input.x != 0.0f)
    {
        last_x = input.x;
    }
    else if (input.y != 0.0f)
    {
        last_x = 0.0f;
    }

    coyote_time_count -= dt;
    jump_buffer_count -= dt;

    if (grounded)
    {
        coyote_time_count   = coyote_time;
        velocity_y          = 0.0f;
        jumping             = false;
    }
    else
    {
        velocity_y  = glm::clamp(velocity_y - GRAVITY, -MAX_FALL_SPEED, MAX_FALL_SPEED);
    }

    // jump button down.
    if ((SPACE_PRESSED == 1) && (SPACE_PRESSED_PREV == 0))
    {
        jump_buffer_count = jump_buffer;
    }

    if ((coyote_time_count > 0.0f) && (jump_buffer_count > 0.0f))
    {
        jump();
    }

    // jump button up.
    if ((SPACE_PRESSED == 0) && (SPACE_PRESSED_PREV == 1))
    {
        float jump_amount   = glm::sqrt(-2.0f * GRAVITY * jump_height * jump_hold);
        jumping             = true;

        if (jumping == true && (velocity_y - jump_amount) > 0)
        {
            velocity_y = jump_amount;
        }
    }

    // get facing angle, only if input is non-zero.
    if (!is_vec2_zero(input))
    {
        input           = glm::normalize(input);
        forward         = glm::normalize(position - camera.get_position(position));
        forward.y       = 0.0f;
        
        // so this actually is the forward vector, ie, from the player AWAY FROM the camera, not towards it.
        // then what needs to happen is that the forward vector is then rotated 
        float angle         = atan2f(input.x, input.y) + atan2f(forward.x, forward.z);  // get rotation angle from input and forward.
        angle_facing        = atan2(sin(angle), cos(angle));                            // wrap rotation from -pi to pi.
        target_animation    = ANIMATION_RUN;                                            // moving, so set to movement animation.
    }
    else
    {
        target_animation    = ANIMATION_IDLE;   // no input, so idle animation.
    }

    // set animation to jump if falling OR jumping.
    if ((coyote_time_count < -coyote_time) || jumping)
    {
        target_animation = ANIMATION_JUMP;
    }

    movement_h      = glm::mix(movement_h,        glm::length(input)  * MAX_SPEED     * dt,   ACCEL * dt);
    animation_speed = glm::mix(animation_speed,   glm::length(input)  * ANIM_SPEED    * dt,   ACCEL * dt);
    
    // some weirdness here, i think bcos we get the slope using the colliders position *before* movement is done, so its like 1 frame behind
    // i think the slope adjustment has to happen after the initial, non adjusted movement.
    // you do the first movement, then after the collision is resolved etc, move from the new position *according to the ground angle at the point of the resolved collision*

    /*
    i think the weirdness is down to the y values. seems like it's sort of trying to move along the y value upwards past where
    it should stop.
    
    */


    vec3 direction      = glm::normalize(rotate(glm::vec3(0.0f, 0.0f, 1.0f), angle_facing - glm::half_pi<float>(), up));
    vec3 horizontal     = glm::normalize(glm::cross(direction, slope_angle)) * movement_h;
    vec3 vertical       = (vec3(0.0f, velocity_y, 0.0f) + slope_angle) * dt;
    move(horizontal + vertical, level, skybox, bgm);

    // out of bounds collision / respawn.
    if (collider[COLLIDER_MAIN].position.y < -100.0f)
    {
        camera.distance_offset = 6.0f;
        respawn(level, skybox, bgm);
    }

    // update camera & model.
    camera_lookat   = vec3(position.x, position.y + (HEIGHT * 0.8f), position.z);
    auto_cam_yaw    = glm::mix(auto_cam_yaw, glm::sqrt(glm::length(glm::vec2(position.x - prev.x, position.z - prev.z))) * movement_h * last_x, 1.0f);
    model_rotation  = glm::slerp(model_rotation, glm::quat(vec3(0.0f, angle_facing, 0.0f)), TURN_SPEED * dt);
    model.update_animations(ANIM_SPEED * dt, target_animation);
}

// draw player.
void Player::draw(int mesh_shader, int line_shader, Camera camera, bool draw_collider)
{
    model.draw(position, model_rotation, scale, mesh_shader, camera, colour);
    
    // option to draw colliders.
    if (draw_collider)
    {
        for (size_t i = 0; i < collider.size(); ++i)
        {
            collider[i].draw(line_shader, camera);
        }
    }
}

// load level and set player position.
void Player::respawn(std::unique_ptr<Level> &level, Skybox &skybox, Sound &bgm)
{
    if (level->index != current_level)
    {
        level = std::make_unique<Level>();
        level->load_level(current_level, skybox, bgm);
    }
    
    move(glm::vec3(respawn_position.x, respawn_position.y + 10.0f, respawn_position.z) - position, level, skybox, bgm);
}

void Player::jump()
{
    // std::cout << position.x << ", " << position.y << ", " << position.z << "\n";
    if (model.animations.size() > ANIMATION_JUMP)
    {
        model.animations[ANIMATION_JUMP].reset();
    }
    sound.position      = 0;    // reset jump sound to beginning (ie play it).
    coyote_time_count   = 0.0f;
    jump_buffer_count   = 0.0f;
    velocity_y          = glm::sqrt(-100.0f * -GRAVITY * jump_height);
    jumping             = true;
}

// move player in a direction, and calculate collision to adjust.
void Player::move(glm::vec3 movement, std::unique_ptr<Level> &level, Skybox &skybox, Sound &bgm)
{
    // first move the collider to the desired position.
    collider[COLLIDER_MAIN].move(position + movement);
    grounded = false;

    for (int i = 0; i < MAX_COLLISION_CHECKS; ++i)
    {
        int collision_count = 0;
        glm::vec3 to_move = glm::vec3(0.0f);
        std::vector<Collision> collisions;
        float deepest = 0.0f;

        for (size_t j = 0; j < level->colliders.size(); ++j)
        {
            Collision collision = get_collision(&collider[COLLIDER_MAIN], &*level->colliders[j]);
            
            if (collision.collided)
            {
                if (level->colliders[j]->is_trigger)
                {
                    // can make this a conditional at some point to have more than 1 type of trigger response.
                    // rn it is just level changing.
                    current_level       = level->colliders[j]->target_level;
                    respawn_position    = level->colliders[j]->spawn;
                    respawn(level, skybox, bgm);
                }
                else
                {
                    // check if the collision is sufficiently below the player.
                    if (glm::angle(glm::normalize(collision.normal), up) > GROUND_MIN)
                    {
                        grounded = true;
                    }

                    // move collider outside of collision, then increment count to check the new position.
                    // collider[COLLIDER_MAIN].move(collider[COLLIDER_MAIN].position - (collision.normal * collision.depth));


                    // if (collision.depth > deepest)
                    // {
                    //     deepest = collision.depth;
                        to_move += (collision.normal * collision.depth);
            
                    // }
                    
                    
                }
                // collisions.push_back(collision);
                ++collision_count;
            }  
        }


        // if (collisions.empty())
        // {
        //     break;
        // }

        // for (const Collision& collision : collisions)
        // {
        //     collider[COLLIDER_MAIN].move(collider[COLLIDER_MAIN].position - (collision.normal * collision.depth));
        // }

        collider[COLLIDER_MAIN].move(collider[COLLIDER_MAIN].position - to_move);

        if (collision_count == 0)
        {
            break;
        }
        // if (i > 1)
        // {
        //     std::cout << i << "\n";
        // }

    }

    // update the 'real' position to the colliders position.
    position = collider[COLLIDER_MAIN].position;
    collider[COLLIDER_GROUND].move(position - (up * GROUND_DEPTH));
}

// this seems pretty solid atm, i don't think this is causing any issues with ground checks.
// returns the angle of collider that is intersecting with the players ground check collider.
glm::vec3 Player::get_slope(std::vector<std::unique_ptr<Collider>> &colliders)
{
    // default value of the floor is vec3(0, -1, 0). -- could make a 'floor' vec3 const?
    glm::vec3 slope_angle = -up;
    if (grounded)
    {
        for (int i = 0; i < MAX_COLLISION_CHECKS; ++i)
        {
            int collision_count = 0;
            for (size_t j = 0; j < colliders.size(); ++j)
            {
                Collision collision = get_collision(&collider[COLLIDER_GROUND], &*colliders[j]);
                if (collision.collided)
                {
                    if (glm::angle(glm::normalize(collision.normal), up) > GROUND_MIN)
                    {
                        slope_angle = collision.normal;
                        collision_count++;
                    }
                }
            }
            if (collision_count == 0)
            {
                break;
            }
        }
    }

    return glm::normalize(slope_angle);
}