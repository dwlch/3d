#pragma once


#include "collision.hpp"
#include "npc.hpp"
#include "draw.hpp"
#include "shader.hpp"
#include "camera.hpp"

#include <memory>       // for collision array.
#include <vector>

struct Level
{
    Model model;                // level geometry is a single gltf rn.
    Skybox skybox;              // unique skybox per level.
    glm::vec3 light_direction;  // per-level lighting.
    int current_level;          // store int of current level to prevent unnecessary loading.

    std::vector<std::unique_ptr<Collider>> colliders;   // vector array of colliders to test against player in update.
    std::vector<std::unique_ptr<Collider>> triggers;    // vector array of triggers in the level.
    std::vector<Npc> npcs;                              // npc array

    Level(int initial_level);
    void load(int level_index);
    void update(int target_level);
    void draw(Shader level_shader, Shader skybox_shader, Shader npc_shader, Shader line_shader, Camera camera);
};