#include "npc.hpp"

#include <iostream>

void Textbox::draw(float x, float y, int shader)
{
    text.draw(content, x, y, shader);
}


Npc::Npc(std::string model_name, glm::vec3 position, glm::quat rotation, std::string display_name)
{
    Npc::position       = position;
    Npc::model_rotation = rotation;
    model.load_from_file(model_name);

    name                = display_name;
    dialogue            = name + std::string(": ") + std::to_string(std::rand());
}

void Npc::update(float dt)
{
    model.update_animations(animation_speed * dt, target_animation);
}

void Npc::draw(int mesh_shader, int line_shader, Camera camera, bool draw_collider)
{
    model.draw(position, model_rotation, scale, mesh_shader, camera, colour);


    
}