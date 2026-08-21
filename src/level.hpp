#pragma once


#include "collision.hpp"
#include "npc.hpp"
#include "draw.hpp"
#include "gltf.hpp"
#include "shader.hpp"
#include "camera.hpp"
#include "audio.hpp"
#include "defines.hpp"

#include <memory>       // for collision array.
#include <vector>

struct Level : public glTF
{
    std::vector<std::unique_ptr<Collider>> colliders;   // vector array of colliders to test against player in update.
    std::vector<std::unique_ptr<Collider>> triggers;    // vector array of triggers in the level.
    std::vector<Npc> npcs;                              // npc array
    // std::vector<Sound> sounds;
    // Skybox skybox;

    glm::vec3 light;    // could be part of skybox? or just a Light struct itself.
    int index;          // index of the currently loaded level (to avoid unnecessary loading).

    Level() {};
    void load_level(int level_index, Skybox &skybox, Sound &bgm);
    void get_extras(const tinygltf::Node *in_node, Node *out_node) override;
};