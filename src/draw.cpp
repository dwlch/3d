#include "draw.hpp"
#include "defines.hpp"                  // window dimensions.
#include <filesystem>                   // file extension parsing
#include <iostream>                     // std::cout etc.
#include <typeinfo>
#include <glm/gtc/type_ptr.hpp>         // get type of pointer for shaders.
#include <stb_image.h>                  // load images (include seperately from tinygltf).
#include <array>
#include <random>

#include <sstream>
#include <string>

#define MODELS_PATH     "./assets/models/"
#define TEXTURES_PATH   "./assets/textures/"

// very simple circle for collision shapes. 
Circle::Circle(float radius)
{
    // make vertices along a circle. increment = resolution?
    for (float i = 0; i < 360; ++i)
    {
        Vertex vertex;
        vertex.position         = glm::vec3(sinf(glm::radians(i)) * radius, 0.0f, cosf(glm::radians(i)) * radius);
        vertex.normal           = vertex.position;
        vertex.color            = glm::vec3(0.0f);
        vertex.texUV            = glm::vec2(0.0f, 0.0f);
        vertex.joint_indices    = glm::vec4(1.0f);
        vertex.joint_weights    = glm::vec4(1.0f);

        mesh.vertex_buffer.push_back(vertex);
        mesh.index_buffer.push_back(i);
    }

    // fill joint matrix with placeholder values.
    for (size_t i = 0; i < MAX_JOINTS; ++i)
    {
        joint_matrix[i] = glm::mat4(1.0f);
    }

    bind_buffers(mesh.VAO, mesh.VBO, mesh.EBO, mesh.vertex_buffer, mesh.index_buffer);
}

// draw circle.
void Circle::draw(glm::vec3 position, int shader, Camera camera, glm::vec3 colour)
{
    glm::quat rotation  = glm::quat(glm::vec3(0.0f));
    glm::vec3 scale     = glm::vec3(1.0f);
    glm::mat4 mvp       = translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);

    // draw mesh with GL_LINES.
    glUseProgram(shader);    // activate the shader being used to draw this model. (potential for multiple shaders per model?)
    
    // per mesh shader updates; colour and combined matrix transformation.
    glUniform3fv(glGetUniformLocation(shader, "albedo"), 1, glm::value_ptr(colour));   
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, glm::value_ptr(camera.mvp));
    glUniformMatrix4fv(glGetUniformLocation(shader, "joint_matrices"), MAX_JOINTS, GL_FALSE, glm::value_ptr(joint_matrix[0]));
    
    glBindVertexArray(mesh.VAO);     // bind the VBO with the vertexes from the mesh.
    glUniformMatrix4fv(glGetUniformLocation(shader, "mvp"), 1, GL_FALSE, glm::value_ptr(mvp));

    // set polygon mode and then draw elements.
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(1.0f);
    glDrawElements(GL_LINE_LOOP, mesh.index_buffer.size() * sizeof(mesh.index_buffer[0]), GL_UNSIGNED_INT, 0);

    // unbind vertex array and texture.
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
}


// used for post-process rendering.
ScreenTexture::ScreenTexture()
{
   float vertices[] = { 
        // position.    // texCoord.
        // tri 1.
        -1.0f,  1.0f,   0.0f, 1.0f,
        -1.0f, -1.0f,   0.0f, 0.0f,
         1.0f, -1.0f,   1.0f, 0.0f,
        // tri 2.
        -1.0f,  1.0f,   0.0f, 1.0f,
         1.0f, -1.0f,   1.0f, 0.0f,
         1.0f,  1.0f,   1.0f, 1.0f
    };

    // generate VAO.
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // generate VBO, bind, then pass vertices vector.
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);

    // link attribs.
    link_attrib(VBO, 0, 2, GL_FLOAT, 4 * sizeof(float), (void*)(0));                    // positions (vec2).
    link_attrib(VBO, 1, 2, GL_FLOAT, 4 * sizeof(float), (void*)(2 * sizeof(float)));    // texcoords (vec2).
    
    // render buffer object.
    glGenRenderbuffers(1, &RBO);
    glBindRenderbuffer(GL_RENDERBUFFER, RBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, WINDOW_WIDTH, WINDOW_HEIGHT);

    // attach texture and RBO to FBO.
    glGenFramebuffers(1, &screen_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, screen_FBO);
    glGenTextures(2, color_buffers);

    for (int i = 0; i < 2; ++i)
    {
        glBindTexture(GL_TEXTURE_2D, color_buffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, color_buffers[i], 0);
    }
    
    unsigned int attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, attachments);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);

    // check if any errors with framebuffer.
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "FRAMEBUFFER NOT COMPLETE!" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);


    // ping-pong-framebuffer for blurring
    glGenFramebuffers(2, pingpong_FBO);
    glGenTextures(2, pingpong_buffers);
    for (unsigned int i = 0; i < 2; ++i)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpong_FBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpong_buffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpong_buffers[i], 0);
        // also check if framebuffers are complete (no need for depth buffer)
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Framebuffer not complete!" << std::endl;
    }

    bloom = false;

}

// draw framebuffer texture to screen.
void ScreenTexture::draw(int screen_shader, int blur_shader)
{
    bool horizontal = true;
    if (bloom)
    {
        
        bool first_iteration    = true;
        unsigned int amount     = 10;

        glUseProgram(blur_shader);
        glBindVertexArray(VAO);

        for (unsigned int i = 0; i < amount; ++i)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpong_FBO[horizontal]);
            glUniform1i(glGetUniformLocation(blur_shader, "horizontal"), horizontal);
            glBindTexture(GL_TEXTURE_2D, first_iteration ? color_buffers[1] : pingpong_buffers[!horizontal]);  // bind texture of other framebuffer (or scene if first iteration)

            // draw pingpong buffer.
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDrawArrays(GL_TRIANGLES, 0, 6);


            horizontal = !horizontal;
            if (first_iteration)
            {
                first_iteration = false;
            }
        }
    }

    // clear buffers etc before drawing.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // apply shader and bind texture.
    glUseProgram(screen_shader);
    glBindVertexArray(VAO);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, color_buffers[0]);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, pingpong_buffers[!horizontal]);

    // send gamma to post process shader.
    glUniform1i(glGetUniformLocation(screen_shader, "screen_texture"), 0);
    glUniform1i(glGetUniformLocation(screen_shader, "bloom"), 1);
    glUniform1f(glGetUniformLocation(screen_shader, "gamma"), gamma);

    // draw the framebuffer.
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glEnable(GL_DEPTH_TEST);


    // unbind vertex array and texture.
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
}


Skybox::Skybox(std::string filename)
{
    float vertices[] = {         
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    // generate VAO.
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // generate VBO, bind, then pass vertices vector.
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);

    // link attribs.
    link_attrib(VBO, 0, 3, GL_FLOAT, 3 * sizeof(float), (void*)(0)); // positions (vec3).

    // change this to just use the vertices directly so i dont need a literal cube file Lol.
    cube_mesh.load_from_file("cube.glb");
    name                        = filename;
    std::string current_skybox  = TEXTURES_PATH + std::string("skybox_" + name + "/");
    

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &ID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, ID);

    // set params for cubemap texture.
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    int width;
    int height;
    int component;

    for (size_t i = 0; i < filenames.size(); ++i)
    {
        unsigned char *data = stbi_load((current_skybox + filenames[i] + std::string(".jpg")).data(), &width, &height, &component, STBI_rgb_alpha);
        if(!data)
        {
            data = stbi_load((current_skybox + filenames[i] + std::string(".png")).data(), &width, &height, &component, STBI_rgb_alpha);
        }
        
        if(!data)
        {
            // throw(std::string("Failed to load texture"));
            std::cout << "Failed to load cubemap texture: " << filenames[i] << "\n";
            stbi_image_free(data);
        }

        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
        
        // std::cout << "Loaded cubemap texture: " << filenames[i] << "\n";
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void Skybox::draw(glm::vec3 position, int shader, Camera camera)
{
    glm::mat4 transform = translate(glm::mat4(1.0f), camera.get_position(position)) * glm::mat4_cast(glm::quat(glm::vec3(0.0f))) * glm::scale(glm::mat4(1.0f), glm::vec3(camera.FAR_PLANE));
    Frustum frustum(camera.mvp);

    glCullFace(GL_FRONT);

    glUseProgram(shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, ID);
    glUniform1i(glGetUniformLocation(shader, "cubemap_texture"), 0);
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, glm::value_ptr(camera.mvp));
    glUniformMatrix4fv(glGetUniformLocation(shader, "mvp"), 1, GL_FALSE, glm::value_ptr(transform));
    int whatever = 0;

    // loop through all nodes in the model.
    for (auto &node : cube_mesh.nodes)
    {
        cube_mesh.draw_node(*node, GL_TRIANGLES, transform, shader, frustum, whatever);
    }

    // cube_mesh.draw(glm::vec3(0.0f), glm::quat(glm::vec3(0.0f)), glm::vec3(1.0f), shader, camera, glm::vec3(1.0f));

    glCullFace(GL_BACK);
}



// drawing image to UI etc.
Image2D::Image2D(std::string filename)
{
    unsigned char *data = stbi_load((TEXTURES_PATH + filename).data(), &width, &height, &component, STBI_rgb_alpha);
    if (!data)
    {
        throw(std::string("Failed to load texture"));
    }

    // if ((width & (width - 1)) != 0 || (height & (height - 1)) != 0)
    // {
	// 	fprintf(stderr, "WARNING: texture %s is not power-of-2 dimensions\n", filename.data());
	// }

    float vertices[] = {
        // position.    // texCoord.
        // tri 1.
        -1.0f,  1.0f,   0.0f, 0.0f,
        -1.0f, -1.0f,   0.0f, 1.0f,
         1.0f, -1.0f,   1.0f, 1.0f,

        // tri 2.
        -1.0f,  1.0f,   0.0f, 0.0f,
         1.0f, -1.0f,   1.0f, 1.0f,
         1.0f,  1.0f,   1.0f, 0.0f
    };

    // generate VAO.
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // generate VBO, bind, then pass vertices vector.
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);

    // link attribs.
    link_attrib(VBO, 0, 2, GL_FLOAT, 4 * sizeof(float), (void*)(0));                    // positions (vec2).
    link_attrib(VBO, 1, 2, GL_FLOAT, 4 * sizeof(float), (void*)(2 * sizeof(float)));    // texcoords (vec2).
    
    // generate texture with parameters.
    glGenTextures(1, &ID);
    glBindTexture(GL_TEXTURE_2D, ID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // unbind texture.
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
}

void Image2D::draw(float x, float y, float scale, int shader)
{
    glDisable (GL_DEPTH_TEST);
    glUseProgram(shader);
    glBindVertexArray(VAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ID);

    // send all parameters to shader uniforms.
    glUniform1f(glGetUniformLocation(shader, "x_pos"), x);
    glUniform1f(glGetUniformLocation(shader, "y_pos"), y);
    glUniform1f(glGetUniformLocation(shader, "scale"), scale);
    glUniform1f(glGetUniformLocation(shader, "window_width"), (float)WINDOW_WIDTH);
    glUniform1f(glGetUniformLocation(shader, "window_height"), (float)WINDOW_HEIGHT);
    glUniform1f(glGetUniformLocation(shader, "img_width"), width);
    glUniform1f(glGetUniformLocation(shader, "img_height"), height);
    
    // enable alpha blending for transparency.
    glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // draw image quad.
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisable(GL_BLEND);    // turn off blending.
    glEnable(GL_DEPTH_TEST);

    // unbind VAO and texture.
    glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArray(0);
}

float glyph_y_offsets[256]  = { 0.5f };
float glyph_widths[256]     = { 0.5f };

// After tweaking, save these values to a file
void tweak_glyphs()
{
	// // lower down glyhs by some factor
	// for (int i = 0; i < 255; i++)
    // {
	// 	// default lower-case to half-size spacing
	// 	if (i >= 'a' && i <= 'z')
    //     {
	// 		glyph_y_offsets[i] = 0.6f;
	// 	}
    //     else if (i >= 'A' && i <= 'Z')
    //     {
	// 		glyph_y_offsets[i] = 0.2f;
	// 	}
    //     else
    //     {
    //         glyph_y_offsets[i] = 0.5f;
    //     }
	// }

	// glyph_y_offsets['b'] = 0.4f;
	// glyph_y_offsets['d'] = 0.4f;
	// glyph_y_offsets['f'] = 0.4f;
	// glyph_y_offsets['g'] = 0.5f;
	// glyph_y_offsets['h'] = 0.3f;
	// glyph_y_offsets['i'] = 0.5f;
	// glyph_y_offsets['j'] = 0.4f;
	// glyph_y_offsets['k'] = 0.3f;
	// glyph_y_offsets['l'] = 0.3f;
	// glyph_y_offsets['p'] = 0.5f;
	// glyph_y_offsets['q'] = 0.5f;
	// glyph_y_offsets['s'] = 0.4f;
	// glyph_y_offsets['t'] = 0.3f;
	// glyph_y_offsets['!'] = 0.1f;
    // glyph_y_offsets['?'] = 0.1f;
	// glyph_y_offsets[','] = 1.0f;
    // glyph_y_offsets['.'] = 1.0f;
    // glyph_y_offsets['('] = 0.5f;
    // glyph_y_offsets[')'] = 0.5f;

	// // reduce spacing after glyph
	// for (int i = 0; i < 255; i++)
    // {
	// 	// default lower-case to half-size spacing
	// 	if (i >= 'a' && i <= 'z')
    //     {
	// 		glyph_widths[i] = 0.5f;
	// 	}
    //     else if (i >= 'A' && i <= 'Z')
    //     {
	// 		glyph_widths[i] = 0.7f;
	// 	}
    //     else
    //     {
	// 		glyph_widths[i] = 0.7f;
	// 	}
	// }

    // 
    // float glyph_size = 32.0f;

    // // punctuation.
    // glyph_widths[' '] = 5.0f / glyph_size;
	// glyph_widths['!'] = 5.0f / glyph_size;
    // glyph_widths['"'] = 7.0f / glyph_size;

    // // numbers.
    // glyph_widths['0'] = 10.0f / glyph_size;
    // glyph_widths['1'] = 9.0f / glyph_size;
    // glyph_widths['2'] = 10.0f / glyph_size;
    // glyph_widths['3'] = 9.0f / glyph_size;
    // glyph_widths['4'] = 9.0f / glyph_size;
    // glyph_widths['5'] = 10.0f / glyph_size;
    // glyph_widths['6'] = 10.0f / glyph_size;
    // glyph_widths['7'] = 9.0f / glyph_size;
    // glyph_widths['8'] = 9.0f / glyph_size;
    // glyph_widths['9'] = 10.0f / glyph_size;

    // glyph_widths[':'] = 4.0f / glyph_size;
    // glyph_widths[';'] = 4.0f / glyph_size;

    // // capital letters.
    // glyph_widths['A'] = 11.0f / glyph_size;
    // glyph_widths['B'] = 12.0f / glyph_size;
    // glyph_widths['C'] = 12.0f / glyph_size;
    // glyph_widths['D'] = 12.0f / glyph_size;
    // glyph_widths['E'] = 11.0f / glyph_size;
    // glyph_widths['F'] = 10.0f / glyph_size;
    // glyph_widths['G'] = 13.0f / glyph_size;
    // glyph_widths['H'] = 12.0f / glyph_size;
    // glyph_widths['I'] = 5.0f / glyph_size;
    // glyph_widths['J'] = 9.0f / glyph_size;
    // glyph_widths['K'] = 12.0f / glyph_size;
    // glyph_widths['L'] = 10.0f / glyph_size;
    // glyph_widths['M'] = 15.0f / glyph_size;
    // glyph_widths['N'] = 12.0f / glyph_size;
    // glyph_widths['O'] = 13.0f / glyph_size;
    // glyph_widths['P'] = 12.0f / glyph_size;
    // glyph_widths['Q'] = 13.0f / glyph_size;
    // glyph_widths['R'] = 12.0f / glyph_size;
    // glyph_widths['S'] = 11.0f / glyph_size;
    // glyph_widths['T'] = 10.0f / glyph_size;
    // glyph_widths['U'] = 12.0f / glyph_size;
    // glyph_widths['V'] = 10.0f / glyph_size;
    // glyph_widths['W'] = 16.0f / glyph_size;
    // glyph_widths['X'] = 11.0f / glyph_size;
    // glyph_widths['Y'] = 11.0f / glyph_size;
    // glyph_widths['Z'] = 11.0f / glyph_size;

    // // lowercase letters.
    // glyph_widths['a'] = 11.0f / glyph_size;
    // glyph_widths['b'] = 11.0f / glyph_size;
    // glyph_widths['c'] = 9.0f / glyph_size;
    // glyph_widths['d'] = 12.0f / glyph_size;
    // glyph_widths['e'] = 11.0f / glyph_size;
    // glyph_widths['f'] = 10.0f / glyph_size;
    // glyph_widths['g'] = 13.0f / glyph_size;
    // glyph_widths['h'] = 12.0f / glyph_size;
    // glyph_widths['i'] = 5.0f / glyph_size;
    // glyph_widths['j'] = 9.0f / glyph_size;
    // glyph_widths['k'] = 12.0f / glyph_size;
    // glyph_widths['l'] = 10.0f / glyph_size;
    // glyph_widths['m'] = 15.0f / glyph_size;
    // glyph_widths['n'] = 12.0f / glyph_size;
    // glyph_widths['o'] = 13.0f / glyph_size;
    // glyph_widths['p'] = 12.0f / glyph_size;
    // glyph_widths['q'] = 13.0f / glyph_size;
    // glyph_widths['r'] = 12.0f / glyph_size;
    // glyph_widths['s'] = 11.0f / glyph_size;
    // glyph_widths['t'] = 10.0f / glyph_size;
    // glyph_widths['u'] = 12.0f / glyph_size;
    // glyph_widths['v'] = 10.0f / glyph_size;
    // glyph_widths['w'] = 16.0f / glyph_size;
    // glyph_widths['x'] = 11.0f / glyph_size;
    // glyph_widths['y'] = 11.0f / glyph_size;
    // glyph_widths['z'] = 11.0f / glyph_size;

	
}

// drawing image to UI etc.
Text::Text(std::string font, int glyph_size) : glyph_size(glyph_size)
{
    unsigned char *data = stbi_load((TEXTURES_PATH + font).data(), &width, &height, &component, STBI_rgb_alpha);
    if (!data)
    {
        throw(std::string("Failed to load texture"));
    }

    // generate VAO & VBOs.
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &position_VBO);
    glGenBuffers(1, &texture_VBO);
    
    // generate texture with parameters.
    glGenTextures(1, &ID);
    glBindTexture(GL_TEXTURE_2D, ID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // unbind texture.
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    // adjust glyph widths/heights on load.
    tweak_glyphs();
}

// https://rosettacode.org/wiki/Word_wrap#C++
const char* wrap(const char *text, size_t line_length)
{
    std::istringstream words(text);
    std::ostringstream wrapped;
    std::string word;

    if (words >> word)
    {
        wrapped << word;
        size_t space_left = line_length - word.length();
        while (words >> word)
        {
            if (space_left < word.length() + 1)
            {
                wrapped << '\n' << word;
                space_left = line_length - word.length();
            }
            else
            {
                wrapped << ' ' << word;
                space_left -= word.length() + 1;
            }
        }
    }
    return wrapped.str().data();
}

// https://github.com/capnramses/antons_opengl_tutorials_book/blob/9a117a649ae4d21d68d2b75af5232021f5957aac/26_bitmap_fonts/main.cpp#L368
void Text::draw(std::string content, float at_x, float at_y, int shader)
{
    if (content.size() == 0)
    {
        // no need to draw text that doesn't exist, so return.
        return;
    }

    int text_speed          = 7;
    
    int box_width           = (scale * std::floor<int>(WINDOW_WIDTH / ((int)scale * glyph_size))) + 1;
    scale                   = scale * glyph_size;
    int cols_count          = width / glyph_size;
    int rows_count          = height / glyph_size;
    float in_x              = -1.0f + (at_x / (float)WINDOW_WIDTH   * 2);
    float in_y              =  1.0f - (at_y / (float)WINDOW_HEIGHT  * 2);
    const float x_align     = in_x; // reference x axis position.
    const char* str         = wrap(content.data(), 90);
    const int len           = strlen(str);
    const size_t size       = len * 12 * sizeof(float);
    std::vector<float> vertices(size);  // prob better to store these as vectors rather than using malloc(?) safer.
    std::vector<float> texcoords(size);

    // std::default_random_engine generator;
    // std::uniform_real_distribution<double> distribution(-0.05, 0.05);

    for (int i = 0; i < position; ++i)
    {
        // get row and column of the ascii character.
		int current_col = (str[i] - ' ') % cols_count;
		int current_row = (str[i] - ' ') / rows_count;

        // texture coords.
        float tex_x1 = current_col  * (1.0 / cols_count);
        float tex_x2 = tex_x1       + (1.0 / cols_count);
        float tex_y1 = current_row  * (1.0 / rows_count);
        float tex_y2 = tex_y1       + (1.0 / rows_count);

        // positions.
        float pos_x1 = in_x;
        float pos_x2 = pos_x1   + scale / (float)WINDOW_WIDTH;
		float pos_y1 = in_y     - scale / (float)WINDOW_HEIGHT * 0.0f;
        float pos_y2 = pos_y1   - scale / (float)WINDOW_HEIGHT;

        // move letter on to the right by glyph width.
        if (i + 1 < position)
        {
			in_x += 0.4f * scale / (float)WINDOW_WIDTH; // * glyph_widths[str[i]]
		}

        // vertex positions to draw char at.
        vertices.insert(vertices.begin() + (i * 12), {
            pos_x1, pos_y1, pos_x1, pos_y2, pos_x2, pos_y2, // tri 1.
            pos_x1, pos_y1, pos_x2, pos_y2, pos_x2, pos_y1  // tri 2.
        });

        // coords in texture corresponding to the char.
        texcoords.insert(texcoords.begin() + (i * 12), {
            tex_x1, tex_y1, tex_x1, tex_y2, tex_x2, tex_y2, // tri 1
            tex_x1, tex_y1, tex_x2, tex_y2, tex_x2, tex_y1  // tri 2.
        });

        // linebreak.
        if (str[i] == '\n')
        {
            in_x = x_align;
            in_y -= scale / (float)WINDOW_HEIGHT;
        }
    }

    // after drawing, increment position for gradual drawing of text.
    if (position < len)
    {
        if (position + text_speed > len)
        {
            position = len;
        }
        else
        {
            position += text_speed;
        }
    }
    

    // bind VAO and both VBOs.
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, position_VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size(), vertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, texture_VBO);
    glBufferData(GL_ARRAY_BUFFER, texcoords.size(), texcoords.data(), GL_DYNAMIC_DRAW);

    // link attribs.
    link_attrib(position_VBO,   0, 2, GL_FLOAT, 0, NULL); // positions (vec2).
    link_attrib(texture_VBO,    1, 2, GL_FLOAT, 0, NULL); // texcoords (vec2).
    
    // DRAW.
    // enable alpha blending for transparency.
    glUseProgram(shader);
    glDisable (GL_DEPTH_TEST);
    glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // draw image quad.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ID);
	glDrawArrays(GL_TRIANGLES, 0, len * 6);

    // turn off blending.
    glDisable(GL_BLEND);    
    glEnable(GL_DEPTH_TEST);

    // unbind VAO and texture.
    glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArray(0);
}