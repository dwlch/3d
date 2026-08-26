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
    std::vector<Npc> npcs;                              // npc array
    
    /*
    don't think i can have sound and skybox here bcos they need to be independent of the level to prevent unnecessary loading.
    collision and npcs are always per-level though so they make sense to be tied to the level struct.
    */

    glm::vec3 light;    // could be part of skybox? or just a Light struct itself.
    int index;          // index of the currently loaded level (to avoid unnecessary loading).

    Level() {};
    void load_level(int level_index, Skybox &skybox, Sound &bgm);
    void get_extras(const tinygltf::Node *in_node, Node *out_node) override;

    ~Level() override
    { 
        std::cout << "Level " << index << " unloaded.\n";
    };
};