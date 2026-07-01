#include "npc.hpp"

#include <iostream>

Npc::Npc(std::string model_name, glm::vec3 position)
{
    Npc::position   = position;
    model           = Model(model_name);
}

void Npc::update(double dt)
{
    model.update_animations(animation_speed * dt, target_animation);
    // position.x = position.x + 0.01f;
    // position.z = position.z + 0.01f;


}

void Npc::draw(Shader mesh_shader, Shader line_shader, Camera camera, bool draw_collider)
{
    model.draw(position, model_rotation, scale, mesh_shader, camera, colour);
}