#include "player.hpp"
#include "defines.hpp"
#include "utility.hpp"
#include "input.hpp"
#include <iostream>

using glm::vec2;
using glm::vec3;

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
    current_level       = 1;
    jump_sound          = Sound("jump_0", SAMPLE_RATE, false, 1.0f);
    boosted_jump_sound  = Sound("jump_1", SAMPLE_RATE, false, 0.1f);
}

float last_x = 0.0f;

void Player::update(std::unique_ptr<Level> &level, Skybox &skybox, Sound &bgm, Camera &camera, float dt)
{




    input               = glm::vec2(glm::clamp(HORIZONTAL + HORIZONTAL_, -1.0f, 1.0f), glm::clamp(VERTICAL + VERTICAL_, -1.0f, 1.0f));
    vec3 slope_angle    = get_slope(level->colliders);
    glm::vec3 prev      = position;
    coyote_time_count   -= dt;
    jump_buffer_count   -= dt;


    

    if (input.x != 0.0f)
    {
        last_x = input.x;
    }
    else if (input.y != 0.0f)
    {
        last_x = 0.0f;
    }

    // reset coyote time and set y velocity to 0 while player is grounded.
    if (grounded)
    {
        coyote_time_count   = coyote_time;
        velocity_y          = 0.0f;
        jumping             = false;
        model.animations[ANIMATION_JUMP].reset();
    }
    else
    {
        // player is in air, so apply gravity to y axis velocity.
        velocity_y = glm::clamp(velocity_y - (GRAVITY * dt), -JUMP_POWER, JUMP_POWER + JUMP_BOOST);
    }

    // jump button down.
    if ((SPACE_PRESSED == 1 && SPACE_PRESSED_PREV == 0) || (GAMEPAD_BUTTON_A == 1 && GAMEPAD_BUTTON_A_PREV == 0))
    {
        jump_buffer_count = jump_buffer;
    }

    if ((coyote_time_count > 0.0f) && (jump_buffer_count >= 0.0f))
    {
        jump(JUMP_POWER, dt);
    }

    // when jump button is released while velocity y is increasing, cut it short.
    if (SPACE_PRESSED == -1 || GAMEPAD_BUTTON_A == -1)
    {
        float reduced_jump = jump_hold * (JUMP_POWER + (GRAVITY * dt));

        if (jumping && (velocity_y - reduced_jump) > 0.0f)
        {
            velocity_y = reduced_jump;
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
    // std::cout << glm::length(input) << "\n";

    movement_h      = glm::mix(movement_h,      glm::length(input) * MAX_SPEED,     ACCEL * dt);
    animation_speed = glm::mix(animation_speed, glm::length(input) * ANIM_SPEED,    ACCEL * dt);
    
    // some weirdness here, i think bcos we get the slope using the colliders position *before* movement is done, so its like 1 frame behind
    // i think the slope adjustment has to happen after the initial, non adjusted movement.
    // you do the first movement, then after the collision is resolved etc, move from the new position *according to the ground angle at the point of the resolved collision*

    /*
    i think the weirdness is down to the y values. seems like it's sort of trying to move along the y value upwards past where
    it should stop.
    
    */

    vec3 direction      = glm::normalize(rotate(glm::vec3(0.0f, 0.0f, 1.0f), angle_facing - glm::half_pi<float>(), up));
    vec3 horizontal     = (glm::normalize(glm::cross(direction, slope_angle))) * (movement_h * dt);
    vec3 vertical       = (slope_angle + vec3(0.0f, velocity_y, 0.0f)) * dt;


    move(horizontal + vertical, level, skybox, bgm, dt);

    // out of bounds collision / respawn.
    if (collider[COLLIDER_MAIN].position.y < -100.0f)
    {
        camera.distance_offset = 6.0f;
        respawn(level, skybox, bgm, dt);
    }

    // update camera & model.
    camera_lookat   = vec3(position.x, position.y + (HEIGHT * 0.8f), position.z);
    auto_cam_yaw    = glm::mix(auto_cam_yaw, glm::sqrt(glm::length(glm::vec2(position.x - prev.x, position.z - prev.z))) * movement_h * last_x * dt, 1.0f);
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
void Player::respawn(std::unique_ptr<Level> &level, Skybox &skybox, Sound &bgm, float dt)
{
    if (level->index != current_level)
    {
        level = std::make_unique<Level>();
        level->load_level(current_level, skybox, bgm);
    }
    
    move(glm::vec3(respawn_position.x, respawn_position.y + 10.0f, respawn_position.z) - position, level, skybox, bgm, dt);
}

void Player::jump(float power, float dt)
{
    // std::cout << position.x << ", " << position.y << ", " << position.z << "\n";
    if (model.animations.size() > ANIMATION_JUMP)
    {
        model.animations[ANIMATION_JUMP].reset();
    }

    // reset jump sound back to 0 (ie play it).
    if (power == JUMP_POWER)
    {
        jump_sound.position = 0;
        jumping             = true;
        coyote_time_count   = 0.0f;
    }
    else if (power == JUMP_POWER + JUMP_BOOST)
    {
        boosted_jump_sound.position = 0;
        jumping                     = false;
        coyote_time_count           -= dt;
    }

    jump_buffer_count   = 0.0f;
    velocity_y          = power + (GRAVITY * dt);
    
}

// move player in a direction, and calculate collision to adjust.
void Player::move(glm::vec3 movement, std::unique_ptr<Level> &level, Skybox &skybox, Sound &bgm, float dt)
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

                switch (level->colliders[j]->type)
                {
                case Collider::Type::SOLID:
                    // check if the collision is sufficiently below the player.
                    if (glm::angle(glm::normalize(collision.normal), up) > GROUND_MIN)
                    {
                        grounded = true;
                    }

                    // if jumping upwards, and you hit head on a ceiling, set upward velocity to 0.
                    if (glm::angle(glm::normalize(collision.normal), up) < 0.5f && velocity_y > 0.0f)
                    {
                        velocity_y = 0.0f;
                    }

                    // move collider outside of collision, then increment count to check the new position.
                    to_move += (collision.normal * collision.depth);
                    break;

                case Collider::Type::WARP:
                    current_level       = level->colliders[j]->target_level;
                    respawn_position    = level->colliders[j]->spawn;
                    respawn(level, skybox, bgm, dt);
                    break;

                case Collider::Type::BOUNCE:
                    grounded            = false;
                    target_animation    = ANIMATION_JUMP;
                    jump(JUMP_POWER + JUMP_BOOST, dt);
                    break;
                
                default:
                    std::cout << "Error: collider has no Type.\n";
                    break;
                }


                // if (level->colliders[j]->is_trigger)
                // {
                //     if (level->colliders[j]->type == 0)
                //     {
                //         current_level       = level->colliders[j]->target_level;
                //         respawn_position    = level->colliders[j]->spawn;
                //         respawn(level, skybox, bgm, dt);
                //     }
                //     else if (level->colliders[j]->type == 1)
                //     {
                //         grounded            = false;
                //         target_animation    = ANIMATION_JUMP;
                //         jump(JUMP_POWER + JUMP_BOOST, dt);
                //     }
                // }
                // else
                // {
                //     // check if the collision is sufficiently below the player.
                //     if (glm::angle(glm::normalize(collision.normal), up) > GROUND_MIN)
                //     {
                //         grounded = true;
                //     }

                //     // move collider outside of collision, then increment count to check the new position.
                //     // collider[COLLIDER_MAIN].move(collider[COLLIDER_MAIN].position - (collision.normal * collision.depth));


                //     // if (collision.depth > deepest)
                //     // {
                //     //     deepest = collision.depth;
                //         to_move += (collision.normal * collision.depth);
            
                //     // }
                    
                    
                // }
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

        // collider[COLLIDER_MAIN].move(collider[COLLIDER_MAIN].position - to_move);
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