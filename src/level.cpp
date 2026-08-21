#include "level.hpp"

#include <filesystem>
#include <iostream>


#define LEVELS_PATH     "./assets/models/levels/"

// check the extras given for certain values to load into the level.
void Level::get_extras(const tinygltf::Node *in_node, Node *out_node)
{
    tinygltf::Value custom_properties = in_node->extras;
    if (custom_properties.IsObject())
    {
        if (custom_properties.Has("npc"))
        {
            npcs.push_back(Npc("npc_" + std::to_string(custom_properties.Get("npc").GetNumberAsInt()) + ".glb", out_node->translation, out_node->rotation, custom_properties.Get("name").Get<std::string>()));
        }

        if (custom_properties.Has("level"))
        {
            if (custom_properties.Has("spawn") && custom_properties.Get("spawn").ArrayLen() == 3)
            {
                MeshCollider *collider  = new MeshCollider(out_node->collision_vertices);
                collider->target_level  = custom_properties.Get("level").GetNumberAsInt();
                collider->spawn         = glm::vec3(
                    (float)custom_properties.Get("spawn").Get(0).GetNumberAsDouble(),
                    (float)custom_properties.Get("spawn").Get(1).GetNumberAsDouble(),
                    (float)custom_properties.Get("spawn").Get(2).GetNumberAsDouble()
                );

                triggers.push_back(std::move(std::unique_ptr<Collider>(collider)));
            }
        }

        if (custom_properties.Has("nonsolid"))
        {
            if (!custom_properties.Get("nonsolid").Get<bool>())
            {
                MeshCollider *collider = new MeshCollider(out_node->collision_vertices);
                colliders.push_back(std::move(std::unique_ptr<Collider>(collider)));
            }
        }
    } 
    else // default to mesh collider on anything untagged.
    {
        MeshCollider *collider = new MeshCollider(out_node->collision_vertices);
        colliders.push_back(std::move(std::unique_ptr<Collider>(collider)));
    }
}

// load a model from a .gltf file (works with both combined and seperate, but not .glb).
void Level::load_level(int level_index, Skybox &skybox, Sound &bgm)
{
    tinygltf::Model glTF_input;         // stores .gltf model reference.
    tinygltf::TinyGLTF glTF_context;    // stores ASCII from file.
    std::string error;                  // outputs warning if fails to load properly.
    std::string warning;                // outputs error if any errors.
    bool loaded             = false;
    std::string filename    = std::string("level_" + std::to_string(level_index) + ".glb");

    if (std::filesystem::path(filename).extension() == ".glb")
    {
        loaded = glTF_context.LoadBinaryFromFile(&glTF_input, &error, &warning, LEVELS_PATH + filename);
    }
    else if (std::filesystem::path(filename).extension() == ".gltf")
    {
        loaded = glTF_context.LoadASCIIFromFile(&glTF_input, &error, &warning, LEVELS_PATH + filename);
    }
    else
    {
        std::cout << "Invalid file extension (only accepts .glb and .gltf)\n";
    }

    if (!error.empty())
    {
        std::cout << "ERR: " << error << "\n";
    }
    if (!warning.empty())
    {
        std::cout << "WARN: " << warning << "\n";
    }

    // store the indices and vertices for all meshes continuously in vectors.
    // atm this is unused, theyre being stored per-mesh rn.
    std::vector<uint32_t> index_buffer;
    std::vector<Vertex> vertex_buffer;

    // if loaded correctly, load file contents.
    if (loaded)
    {
        std::cout << "\nLOADING LEVEL from file: " << filename << "\n";
        index = level_index;

        // load images, materials, textures.
        load_material(glTF_input);

        //glTF_input.scenes[0] = glTF_input.scenes[0] (pretty sure).
        // assert(glTF_input.scenes[0] == glTF_input.scenes[0]);
        const tinygltf::Scene &scene = glTF_input.scenes[0];

        // get level's music.
        if (scene.extras.Has("bgm"))
        {
            std::string this_bgm = std::to_string(scene.extras.Get("bgm").GetNumberAsInt());
            if (bgm.name != this_bgm)
            {
                bgm = Sound(this_bgm, SAMPLE_RATE, true, 0.3f);
            }
        }

        
        // get skybox.
        if (scene.extras.Has("skybox"))
        {
            std::string this_skybox = std::to_string(scene.extras.Get("skybox").GetNumberAsInt());
            if (skybox.name != this_skybox)
            {
                skybox = Skybox(this_skybox);
            }
        }

        // get light direction and colour.
        if (scene.extras.Has("light"))
        {
            assert(scene.extras.Get("light").ArrayLen() == 3);

            light = glm::vec3(
                (float)scene.extras.Get("light").Get(0).GetNumberAsDouble(),
                (float)scene.extras.Get("light").Get(1).GetNumberAsDouble(),
                (float)scene.extras.Get("light").Get(2).GetNumberAsDouble()
            );
        }

        for (size_t i = 0; i < scene.nodes.size(); ++i)
        {
            load_node(glTF_input.nodes[scene.nodes[i]], glTF_input, nullptr, scene.nodes[i], index_buffer, vertex_buffer);
        }

        // load skins and animations.
        load_skins(glTF_input);
        load_animations(glTF_input);

        // calculate initial pose.
        for (auto node : nodes)
		{
			update_joints(node);
		}

        // after loading everything, bind the mesh nodes.
        for (auto &node : nodes)
        {
            bind_node(&*node);
        }
    }

    std::cout << "LEVEL " << std::to_string(level_index) << " LOADED!" << "\n\n";
}