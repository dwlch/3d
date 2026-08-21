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
    collider[COLLIDER_SEARCH]   = CylinderCollider(position - (up * GROUND_DEPTH),  up, GROUND_DEPTH,   RADIUS * 3.0f);

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

void Player::update(std::shared_ptr<Level> &level, Skybox &skybox, Sound &bgm, Camera &camera, float dt)
{
    glm::vec3 prev  = position;
    vec2 input      = vec2(HORIZONTAL, VERTICAL);

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
        // cout << "ground\n";
        coyote_time_count   = coyote_time;
        velocity_y          = 0.0f;
        jumping             = false;
    }
    else
    {
        // cout << "air   \n";
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
        // cout << "jump up" << "\n";
        
        float jump_ammt = glm::sqrt(-2.0f * GRAVITY * jump_height * jump_hold);
        jumping         = true;

        if (jumping == true && (velocity_y - jump_ammt) > 0)
        {
            velocity_y = jump_ammt;
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
        target_animation    = ANIMATION_IDLE;
    }

    // check if falling.
    if ((coyote_time_count < -coyote_time) || jumping)
    {
        target_animation = ANIMATION_JUMP;
    }

    movement_h      = glm::mix(movement_h,        glm::length(input)  * MAX_SPEED     * dt,   ACCEL * dt);
    animation_speed = glm::mix(animation_speed,   glm::length(input)  * ANIM_SPEED    * dt,   ACCEL * dt);

    // unsure why angle facing needs rotating 180 degrees here...
    // oh wait its bcos of the ground angle cross product bit
    // vec3 direction  = glm::normalize(rotate(glm::vec3(0.0f, 0.0f, 1.0f), angle_facing, up));
    // vec3 slope      = glm::normalize(get_slope(colliders));
    // some weirdness here, i think bcos we get the slope using the colliders position *before* movement is done, so its like 1 frame behind
    // i think the slope adjustment has to happen after the initial, non adjusted movement.
    // you do the first movement, then after the collision is resolved etc, move from the new position *according to the ground angle at the point of the resolved collision*

    vec3 direction  = glm::normalize(rotate(glm::vec3(0.0f, 0.0f, 1.0f), angle_facing - glm::half_pi<float>(), up));
    vec3 slope      = glm::normalize(get_slope(level->colliders));
    vec3 horizontal = glm::normalize(glm::cross(direction, slope)) * movement_h;
    vec3 vertical   = (slope + vec3(0.0f, velocity_y, 0.0f)) * dt;

    // cout << "x: " << slope.x << "\ny: " << slope.y << "\nz: " << slope.z << "\n\n";

    // vec3 horizontal = direction * movement_h;
    // vec3 vertical   = vec3(0.0f, velocity_y, 0.0f) * (float)dt;

    // finally move player and calculate collisions.
    // optimise this later.

    move(horizontal + vertical, level, skybox, bgm);


    // if (glm::length(horizontal + vertical) > 0.0f)
    // {
    //     cout << glm::length(vertical) << "\n";
        

    // }
    

    // direction   = glm::normalize(rotate(glm::vec3(0.0f, 0.0f, 1.0f), angle_facing - glm::half_pi<float>() , up));
    // vec3 slope      = glm::normalize(get_slope(colliders));
    // // vec3 adjustment = glm::normalize(glm::cross(direction, slope));
    // vec3 adjustment = glm::cross((slope + vec3(0.0f, velocity_y, 0.0f)) * (float)dt);
    // move(adjustment, colliders, triggers);

    // out of bounds collision / respawn.
    if (collider[COLLIDER_MAIN].position.y < -100.0f)
    {
        camera.distance_offset = 6.0f;
        respawn(level, skybox, bgm);
    }

    if (INPUT_3 == 1)
    {
        camera.distance_offset = 6.0f;
        *level = Level();
        level->load_level(current_level, skybox, bgm);
        move(respawn_position - position, level, skybox, bgm);
    }

    // update model.
    // line                    = Line(rotate(forward, angle_facing, up), 5.0f);
    // float target_test       = glm::mix(prev.y, position.y, 0.000005f);


    camera_lookat           = vec3(position.x, position.y + (HEIGHT * 0.8f), position.z);
    model_rotation          = glm::slerp(model_rotation, glm::quat(vec3(0.0f, angle_facing, 0.0f)), TURN_SPEED * dt);
    model.update_animations(ANIM_SPEED * dt, target_animation);

    
    auto_cam_yaw = glm::mix(auto_cam_yaw, glm::sqrt(glm::length(glm::vec2(position.x - prev.x, position.z - prev.z))) * movement_h * last_x, 1.0f);


}

// draw player.
void Player::draw(Shader mesh_shader, Shader line_shader, Camera camera, bool draw_collider)
{
    model.draw(position, model_rotation, scale, mesh_shader, camera, colour);
    
    // // option to draw colliders.
    // if (draw_collider)
    // {
    //     // line.draw(position, line_rotation, line_shader, camera, colour);
    //     for (size_t i = 0; i < collider.size() - 1; ++i)
    //     {
    //         collider[i].draw(line_shader, camera);
    //     }
    // }
}

// reset player position.
// function cause might want to call from other files mb?? unsure.
void Player::respawn(std::shared_ptr<Level> &level, Skybox &skybox, Sound &bgm)
{
    if (level->index != current_level)
    {
        *level = Level();
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
void Player::move(glm::vec3 movement, std::shared_ptr<Level> &level, Skybox &skybox, Sound &bgm)
{
    // first move the collider to the desired position.
    collider[COLLIDER_MAIN].position = position + movement;
    grounded = false;
    
    for (int i = 0; i < MAX_COLLISION_CHECKS; ++i)
    {
        int collision_count = 0;
        for (size_t j = 0; j < level->colliders.size(); ++j)
        {
            Collision collision = is_collision(&collider[COLLIDER_MAIN], &*level->colliders[j]);
            if (collision.collided)
            {
                // check if the collision is sufficiently below the player.
                if (glm::angle(glm::normalize(collision.normal), up) > GROUND_MIN)
                {
                    // cout << glm::angle(glm::normalize(collision.normal), up) << "\n";
                    grounded = true;
                }

                // move collider outside of collision, then increment count to check the new position.
                collider[COLLIDER_MAIN].position -= (collision.normal * collision.depth);
                // j = 0;
                collision_count++;
            }
        }
        if (collision_count == 0)
        {
            break;
        }
    }

    // update the 'real' position to the colliders position.
    position                            = collider[COLLIDER_MAIN].position;
    collider[COLLIDER_GROUND].position  = collider[COLLIDER_MAIN].position - (up * GROUND_DEPTH);
    collider[COLLIDER_SEARCH].position  = collider[COLLIDER_MAIN].position - (up * GROUND_DEPTH);

    // trigger check.
    bool trigger            = false;
    int trigger_function    = 1;


    for (int i = 0; i < MAX_COLLISION_CHECKS; ++i)
    {
        int collision_count = 0;
        for (size_t j = 0; j < level->triggers.size(); ++j)
        {
            Collision collision = is_collision(&collider[COLLIDER_MAIN], &*level->triggers[j]);
            if (collision.collided)
            {
                current_level       = level->triggers[j]->target_level;
                respawn_position    = level->triggers[j]->spawn;
                trigger             = true;
                collision_count++;
            }
        }
        if (collision_count == 0)
        {
            break;
        }
    }

    if (trigger)
    {
        // std::cout << "Trigger: " << trigger_function << "\n";
        switch (trigger_function)
        {
        case 0:
            jump();
            break;
        case 1:
            respawn(level, skybox, bgm);

            break;

        default:
            cout << "Empty trigger.\n";
            break;
        }
    }
}

// this seems pretty solid atm, i don't think this is causing any issues with ground checks.
// returns the angle of collider that is intersecting with the players ground check collider.
glm::vec3 Player::get_slope(std::vector<std::unique_ptr<Collider>> &colliders)
{
    // default value of the floor is vec3(0, -1, 0). -- could make a 'floor' vec3 const?
    glm::vec3 ground = -up;
    if (grounded)
    {
        for (int i = 0; i < MAX_COLLISION_CHECKS; ++i)
        {
            int collision_count = 0;
            for (size_t j = 0; j < colliders.size(); ++j)
            {
                Collision collision = is_collision(&collider[COLLIDER_GROUND], &*colliders[j]);
                if (collision.collided)
                {
                    if (glm::angle(glm::normalize(collision.normal), up) > GROUND_MIN)
                    {
                        ground = collision.normal;
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

    // std::cout << ground.x << ", " << ground.y << ", " << ground.z << "\n";
    return ground;
}