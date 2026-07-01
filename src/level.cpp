#include "level.hpp"


#include <iostream>

Level::Level(int initial_level)
{
    // load default level on creation.
    load(initial_level);
}

void Level::load(int level_index)
{
    // should add a load screen transition here: 
    // something simple just like a quick fade to black, load the level, fade up from black.
    // wonder how to go abt it tbh, could be like, a value sent to the shader?

    current_level   = level_index;
    model           = Model("scene_" + std::to_string(level_index) + ".gltf");

    // clear all vectors of current level prior to loading.
    npcs.clear();
    colliders.clear();
    triggers.clear();

    for (auto node : model.nodes)
    {
        for (auto mesh : node->mesh_primitives)
        {
            MeshCollider *collider = new MeshCollider(mesh.vertex_collider_buffer);
            // collider->is_trigger

            collider->spawn         = mesh.spawn;
            collider->target_level  = mesh.target_level;
      

            switch (mesh.type)
            {
            case MeshPrimitive::Type::COLLIDER:
                colliders.push_back(std::move(std::unique_ptr<Collider>(collider)));
                break;

            case MeshPrimitive::Type::TRIGGER:
                triggers.push_back(std::move(std::unique_ptr<Collider>(collider)));
                break;
            // case 2:
            //     // load light position and colour.
            //     // load skybox?
            //     break;
            
            default:
                std::cout << "Mesh missing type (collider, trigger).\n";
                break;
            }
        }
    }

    // need to come up with a way to do this properly...
    // ideas:
    //      - using some kind of npc placeholder in the level file itself (custom properties stuff?).
    //      - 
    npcs.push_back(Npc("player_test.gltf",    glm::vec3(-14.0f, 0.8f, 9.0f)));
    npcs.push_back(Npc("player_test.gltf",    glm::vec3(14.0f, -3.5f, 9.0f)));
    npcs.push_back(Npc("test2.gltf",  glm::vec3(-20.0f, -1.5f, 2.0f)));


    // need to be able to load the skybox texture from the gltf i think.
    // also probably good to have some way to load it from a cubemap image not 6 seperate ones? dunno.
    skybox = Skybox(level_index);



    std::cout << "loaded level: " << current_level << "\n\n";
}

void Level::update(int target_level)
{
    if (target_level != current_level)
    {
        load(target_level);
    }
}

void Level::draw(Shader level_shader, Shader skybox_shader, Shader npc_shader, Shader line_shader, Camera camera)
{
    
    model.draw(glm::vec3(0.0f), glm::quat(glm::vec3(0.0f)), glm::vec3(1.0f), level_shader, camera, glm::vec3(1.0f));
    // model.draw(glm::vec3(0.0f), glm::quat(glm::vec3(0.0f)), glm::vec3(1.0f), line_shader, camera, glm::vec3(1.0f));

    for (size_t i = 0; i < npcs.size(); ++i)
    {
        npcs[i].draw(npc_shader, npc_shader, camera, false);
    }
}