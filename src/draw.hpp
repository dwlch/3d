#pragma once

#include <tiny_gltf.h>  // gltf loader.
#include <string>       // for model filename.
#include <vector>       // for vertices, indices, and textures arrays.
#include <array>
#include <json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "gltf.hpp"
#include "shader.hpp"   // for updating shader uniforms per mesh.
#include "camera.hpp"

// a type of mesh. procedural grid generated based on number of given slices.
struct Circle
{
    MeshPrimitive mesh;
    std::array<glm::mat4, MAX_JOINTS> joint_matrix;

    Circle(float radius);
    void draw(glm::vec3 position, Shader shader, Camera camera, glm::vec3 colour);
};

// basically a quad that draws the scene w/ post-processing added.
struct ScreenTexture
{
    GLuint color_buffers[2];
    GLuint pingpong_buffers[2];

    GLuint VAO;
    GLuint VBO;
    GLuint RBO;

    GLuint screen_FBO;
    GLuint pingpong_FBO[2];
    
    float gamma = 0.8f;
    bool bloom;

    ScreenTexture();
    void draw(Shader &screen_shader, Shader &blur_shader);
};

// drawing 2d text to screen from texture.
struct Image2D
{
    GLuint ID;
    GLuint VAO;
    GLuint VBO;

    // stb image load.
    int width;
    int height;
    int component;

    Shader shader = Shader(GL_FILL, "quad.vert",  "quad.frag");

    Image2D(std::string filename);
    void draw(float x, float y, float scale);
};

struct Text
{
    GLuint ID;
    GLuint VAO;
    GLuint position_VBO;
    GLuint texture_VBO;

    // stb image load.
    int width;
    int height;
    int component;

    float scale;

    // Shader shader = Shader(GL_FILL, "quad.vert",  "quad.frag");

    Text(std::string font, float scale);
    void draw(std::string content, float at_x, float at_y, Shader shader);
};

struct Skybox
{
    GLuint ID;
    GLuint VAO;
    GLuint VBO;

    std::string name;
    
    Model cube_mesh;
    std::array<std::string, 6> filenames = {
        "pos_x", "neg_x",
        "pos_y", "neg_y",
        "pos_z", "neg_z"
    };

    Skybox() {}; // default constructor.
    Skybox(std::string filename);
    void draw(glm::vec3 position, Shader shader, Camera camera);
};