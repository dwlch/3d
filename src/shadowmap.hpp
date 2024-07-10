#pragma once

#include <array>
#include <glad.h>
#include "glm/glm.hpp"
#include "camera.hpp"
#include "shader.hpp"

struct ShadowMap
{
    std::array<GLuint, 3> depth_maps;
    std::array<glm::mat4, 3> cascade_proj;
    std::array<float, 3> cascade_bounds {
        32.0f, 128.0f, 512.0f
    };

    int size;
    GLuint FBO;

    ShadowMap(int size);
    void get_light_projection(Camera &camera, glm::vec3 &light_direction);
    void update_uniforms(Shader &shader, Camera &camera, glm::vec3 &light_pos);
    
};