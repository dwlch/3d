#pragma once

#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "draw.hpp"
#include "gltf.hpp"
#include "audio.hpp"
#include "defines.hpp"

struct Textbox
{
    std::string content;
    Text text = Text(FONT_MAIN, 48.0f);
    void draw(float x, float y, Shader shader);
};

struct Npc
{
    Model model;
    Sound sound;
    // std::array<CylinderCollider, COLLIDER_COUNT> collider;

    glm::vec3 position          = glm::vec3(0.0f);
    glm::vec3 scale             = glm::vec3(1.0f);              // scale of player model.
    glm::vec3 colour            = glm::vec3(1.5f);              // ambient colour of player model, stand out while in shadow etc.
    glm::quat model_rotation    = glm::quat(glm::vec3(0.0f));   // rotation quaternion of model.

    float animation_speed       = 1.2f;
    uint32_t target_animation   = 0;
    std::string name;
    std::string dialogue;

    Npc(std::string model_name, glm::vec3 position, glm::quat rotation, std::string display_name);
    void update(float dt);
    void draw(Shader mesh_shader, Shader line_shader, Camera camera, bool draw_collider);
};