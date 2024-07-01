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
    std::array<float, 4> cascade_bounds {
        1.0f, 20.0f, 80.0f, 400.0f
    };

    int size;
    GLuint FBO;

    ShadowMap(int size);
    glm::mat4 get_cascades(Camera &camera, glm::vec3 &light_direction);
    void update_uniforms(Shader &shader, Camera &camera, glm::vec3 &light_pos);
};