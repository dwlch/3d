#include "npc.hpp"

#include <iostream>

void Textbox::draw(float x, float y, int shader)
{
    text.scale = 2.0f;
    text.draw(content, x, y, shader);

    // int text_speed = 5;
    // if (position < content.length())
    // {
    //     if (position + text_speed > content.length())
    //     {
    //         position = content.length();
    //     }
    //     else
    //     {
    //         position += text_speed;
    //     }
    // }
}


Npc::Npc(std::string model_name, glm::vec3 position, glm::quat rotation, std::string display_name)
{
    Npc::position       = position;
    Npc::model_rotation = rotation;
    model.load_from_file(model_name);

    name        = display_name + std::string(": ");
    dialogue    = std::string("A purple pig and a green donkey flew a kite in the middle of the night and ended up sunburnt. When nobody is around, the trees gossip about the people who have walked under them. The glacier came alive as the climbers hiked closer.");

}

void Npc::update(float dt)
{
    model.update_animations(animation_speed * dt, target_animation);
}

void Npc::draw(int mesh_shader, int line_shader, Camera camera, bool draw_collider)
{
    model.draw(position, model_rotation, scale, mesh_shader, camera, colour);


    
}