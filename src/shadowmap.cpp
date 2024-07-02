#include "shadowmap.hpp"
#include "defines.hpp"

#include <iostream>

#include <glm/gtc/type_ptr.hpp>

ShadowMap::ShadowMap(int size)
{
    ShadowMap::size = size;

    // Depth texture. Slower than a depth buffer, but you can sample it later in your shader
    glGenFramebuffers(1, &FBO);
    glGenTextures(3, depth_maps.data());

    for (unsigned int i = 0; i < depth_maps.size(); i++)
    {
        glBindTexture(GL_TEXTURE_2D, depth_maps[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        float border_colour[]  = {1.0f, 1.0f, 1.0f, 1.0f};
	    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_colour);
    }

    // create framebuffer and texture for shadowmap.
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_maps[0], 0);

    // disable writing to colour buffer.
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    
    // check if any errors with framebuffer.
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "FRAMEBUFFER NOT COMPLETE!" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// calculate orthographic projection for shadow map relative to camera view frustum.
glm::mat4 ShadowMap::get_cascades(Camera &camera, glm::vec3 &light_direction)
{
    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();
    float min_z = std::numeric_limits<float>::max();
    float max_z = std::numeric_limits<float>::lowest();

    // get inverse of the camera view (at specific near and far bounds).
    // unsure on the last 2 params here. near and far plane, but unsure if it needs to be near and far of each cascade or camera itself.
    glm::mat4 proj  = glm::perspective(glm::radians(camera.FOV), camera.aspect, 1.0f, cascade_bounds[2]);
    glm::mat4 inv   = glm::inverse(proj * camera.view);

    // get frustum corners (method from opengl tutorial site)
    std::vector<glm::vec4> corners;
    for (unsigned int x = 0; x < 2; x++)
    {
        for (unsigned int y = 0; y < 2; y++)
        {
            for (unsigned int z = 0; z < 2; z++)
            {
                glm::vec4 pt = inv * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
                corners.push_back(pt / pt.w);
            }
        }
    }

    // get average of all corners as target / centre of frustum.
    glm::vec3 target = glm::vec3(0.0f);
    for (const auto &corner : corners)
    {
        target += glm::vec3(corner.x, corner.y, corner.z);
    }
    
    // get light view matrix respetive to light direction and target of fustrum.
    target /= corners.size();
    glm::mat4 light_view = glm::lookAt(target + light_direction, target, camera.up);
    for (size_t i = 0; i < 8; i++)
    {
        glm::vec4 lightspace = light_view * corners[i];

        min_x = std::min(min_x, lightspace.x);
        max_x = std::max(max_x, lightspace.x);
        min_y = std::min(min_y, lightspace.y);
        max_y = std::max(max_y, lightspace.y);
        min_z = std::min(min_z, lightspace.z);
        max_z = std::max(max_z, lightspace.z);
    }

    // output the light projection.
    glm::mat4 light_proj = glm::ortho(min_x, max_x, min_y, max_y, -max_z, -min_z);
    return light_proj * light_view;
}

void ShadowMap::update_uniforms(Shader &shader, Camera &camera, glm::vec3 &light_pos)
{
    glUseProgram(shader.ID);

    for (int i = 0 ; i < NUM_CASCADES ; i++)
    { 
        // start at texture1 as first texture slot in the shaders is for the mesh material atm.
        glActiveTexture(GL_TEXTURE1 + i);
        glBindTexture(GL_TEXTURE_2D, depth_maps[i]);
        glUniform1i(glGetUniformLocation(shader.ID, std::string("shadow_map[" + std::to_string(i) + "]").c_str()), i + 1);
    }

    // other uniforms.
    glUniform1f(glGetUniformLocation(shader.ID, "camera_distance"), camera.distance_offset);
    glUniform3fv(glGetUniformLocation(shader.ID, "light_pos"), 1, glm::value_ptr(light_pos));
    glUniform3fv(glGetUniformLocation(shader.ID, "cascade_bounds"), 3, cascade_bounds.data());
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "light"), 3, GL_FALSE, reinterpret_cast<GLfloat *>(cascade_proj.data()));
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "camera"), 1, GL_FALSE, glm::value_ptr(camera.mvp));
}