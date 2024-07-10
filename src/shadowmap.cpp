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

glm::vec3 transform_by_matrix(const glm::vec3 &point, const glm::mat4 &matrix)
{
    glm::vec4 temp = matrix * glm::vec4(point, 1.0f);
    return glm::vec3(temp);
}

void ShadowMap::get_light_projection(Camera &camera, glm::vec3 &light_direction)
{
    std::array<float, 4> cascade {
        camera.NEAR_PLANE, cascade_bounds[0], cascade_bounds[1], cascade_bounds[2]
    };

    for (int i = 0; i < NUM_CASCADES; i++)
    {
        // get inverse of the camera view (at each cascades near and far bounds).
        glm::mat4 proj  = glm::perspective(glm::radians(camera.FOV), camera.aspect, cascade[i], cascade[i + 1]);
        glm::mat4 inv   = glm::inverse(proj * camera.view);
        
        // get 8 corners of the frustum in world space.
        std::vector<glm::vec3> corners = {
            glm::vec3(-1.0f,  1.0f, 0.0f),
            glm::vec3( 1.0f,  1.0f, 0.0f),
            glm::vec3( 1.0f, -1.0f, 0.0f),
            glm::vec3(-1.0f, -1.0f, 0.0f),
            glm::vec3(-1.0f,  1.0f, 1.0f),
            glm::vec3( 1.0f,  1.0f, 1.0f),
            glm::vec3( 1.0f, -1.0f, 1.0f),
            glm::vec3(-1.0f, -1.0f, 1.0f),
        };

        // transform corners into light space(?). i think.
        for( int j = 0; j < 8; j++)
        {
            glm::vec4 temp  = inv * glm::vec4(corners[j], 1.0f);
            corners[j]      = glm::vec3(temp / temp.w);
        }

        // get average of all corners as target / centre of frustum.
        glm::vec3 frustum_centre = glm::vec3(0.0f);
        for (int j = 0; j < 8; j++)
        {
            frustum_centre += corners[j];
        }
        frustum_centre *= (1.0f / 8.0f);

        // get the radius of a bounding sphere surrounding the frustum corners.
        float radius = 0.0f;
        for(int j = 0; j < 8; j++)
        {
            float dist  = glm::length(corners[j] - frustum_centre);
            radius      = std::max(radius, dist);
        }
        radius = std::ceil(radius * 16.0f) / 16.0f;

        float texels        = static_cast<float>(size) / (radius * 2.0f);
        glm::mat4 look_at   = glm::lookAt(-light_direction, glm::vec3(0.0f), camera.up) * glm::mat4(texels);
        frustum_centre      = transform_by_matrix(frustum_centre, look_at);
        frustum_centre.x    = glm::floor(frustum_centre.x);
        frustum_centre.y    = glm::floor(frustum_centre.y);
        frustum_centre      = transform_by_matrix(frustum_centre, glm::inverse(look_at));

        // get final matrix for cascade.
        glm::vec3 light_target  = frustum_centre - (light_direction * (radius * 2.0f));
        glm::mat4 light_view    = glm::lookAt(frustum_centre, light_target, camera.up);
        glm::mat4 light_proj    = glm::ortho(-radius, radius, -radius, radius, -radius * 6.0f, radius * 6.0f);
        cascade_proj[i]         = light_proj * light_view;
    }
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
    glUniform1fv(glGetUniformLocation(shader.ID, "cascade_bounds"), 3, reinterpret_cast<GLfloat *>(cascade_bounds.data()));
    glUniform3fv(glGetUniformLocation(shader.ID, "light_pos"), 1, glm::value_ptr(light_pos));
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "light"), 3, GL_FALSE, reinterpret_cast<GLfloat *>(cascade_proj.data()));
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "camera"), 1, GL_FALSE, glm::value_ptr(camera.mvp));
}