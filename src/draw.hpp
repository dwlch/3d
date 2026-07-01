#pragma once

#include <tiny_gltf.h>  // gltf loader.
#include <string>       // for model filename.
#include <vector>       // for vertices, indices, and textures arrays.
#include <array>
#include <json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "shader.hpp"   // for updating shader uniforms per mesh.
#include "camera.hpp"

#define MAX_JOINTS 100

// referenced: https://github.com/SaschaWillems/Vulkan/blob/master/examples/gltfskinning/gltfskinning.cpp
// store vertex information from gltf file.
struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 texUV;
    glm::vec4 joint_indices;    // vertex has at most 4 joint IDs it is connected to.
    glm::vec4 joint_weights;    // for each joint, there is an associated weight value.
};

// store texture, colour etc of mesh.
struct Material
{
    glm::vec4 base_colour = glm::vec4(1.0f);    // material colour, defaults to white.
    uint32_t texture_ID;                        // id of the materials texture.
};

struct MeshPrimitive
{
    // determines the type of the mesh.
    enum Type
    {
        COLLIDER,
        TRIGGER
    };

    



    GLuint VAO; // vertex array object.
    GLuint VBO; // vertex buffer object array.
    GLuint EBO; // element buffer object.

    uint32_t first_index;
    uint32_t index_count;   // number of indices in the mesh.
    int32_t material_index; // index of the mesh's material in the model's materials array.

    std::vector<uint32_t> index_buffer;             // stores the list of indices.
    std::vector<Vertex> vertex_buffer;              // stores the raw vertices.


    // collision info.
    Type type;
    std::vector<glm::vec3> vertex_collider_buffer;  // vertices with node matrix applied, used for collision meshes.

    int target_level = 0;
    glm::vec3 spawn = glm::vec3(0.0f);              // spawn point for level changes.


    // bool is_trigger;    // flag if collider is also a trigger collider? not sure if this should go here or not.
    // maybe this should be an enum rather than a bunch of bools?
    // enum TRIGGER, COLLIDER etc





};

// a type of mesh. procedural grid generated based on number of given slices.
struct Circle
{
    MeshPrimitive mesh;
    std::array<glm::mat4, MAX_JOINTS> joint_matrix;

    Circle(float radius);
    void draw(glm::vec3 position, Shader shader, Camera camera, glm::vec3 colour);
};

struct Line
{
    MeshPrimitive mesh;
    std::array<glm::mat4, MAX_JOINTS> joint_matrix;

    Line(glm::vec3 angle, float length);
    void draw(glm::vec3 position, glm::quat rotation, Shader shader, glm::vec3 colour);
};

struct Frustum
{
    MeshPrimitive mesh;
    std::array<glm::mat4, MAX_JOINTS> joint_matrix;

    Frustum(std::vector<glm::vec4> corners);
    void draw(glm::vec3 position, Shader shader);
};

// a type of mesh. procedural grid generated based on number of given slices.
// struct Grid
// {
//     int slices; // number of grid slices.
//     MeshPrimitive mesh;
//     std::array<glm::mat4, MAX_JOINTS> joint_matrix;

//     Grid(int slices);
//     void draw(Shader shader);
// };

// decribes a single node in the gltf file.
struct Node; // so nodes can have parent nodes.
struct Node
{
    Node *                      parent;
    uint32_t                    index;
    int32_t                     skin = -1;
    std::vector<Node *>         children;
    std::vector<MeshPrimitive>  mesh_primitives;

    glm::vec3                   translation{};
    glm::quat                   rotation{};
    glm::vec3                   scale = glm::vec3(1.0f);
    glm::mat4                   matrix;
    glm::mat4                   get_local_matrix();    
};

// each armature is a collection of nodes.
struct Skin
{
    std::string                         name;
    Node *                              skeleton_root = nullptr;
    std::vector<glm::mat4>              inverse_bind_matrices;
    std::vector<Node *>                 joints;
    std::array<glm::mat4, MAX_JOINTS>   joint_matrix = {glm::mat4(1.0f)};
};


// animation related.
struct AnimationSampler
{
    std::string             interpolation;
    std::vector<float>      inputs;
    std::vector<float>      outputs_float;
    std::vector<glm::vec4>  outputs_vec4;
};

struct AnimationChannel
{
    std::string path;
    Node *      node;
    uint32_t    sampler_index;
};

struct Animation
{
    std::string                     name;
    std::vector<AnimationSampler>   samplers;
    std::vector<AnimationChannel>   channels;
    float start                     = std::numeric_limits<float>::max();
    float end                       = std::numeric_limits<float>::min();
    float current_time              = 0.0f;
    bool loop                       = true;
    void reset();
};


// model class which contains a number of meshes which are drawn individually.
// could make this like the colliders and have inheritence so can have mesh, circle, frustum etc.
struct Model
{
    std::vector<Node *>     nodes;      // a node has a mesh and inheritence hierarchy.
    std::vector<Material>   materials;  // materials (colour, texture, can add more stuff like normals).
    std::vector<Skin>       skins;      // armature per mesh? i think that's how it works.
    std::vector<Animation>  animations; // each animation accessed by the index.

    uint32_t active_animation = 0;      // general format: 0 = idle, 1 = walk, 2 = jump/fall.

    Model() {}; // default constructor.
    Model(std::string filename);
    Node *find_node(Node *parent, uint32_t index);
    Node *node_from_index(uint32_t index);
    void bind_node(Node *node);
    void draw_node(Node node, GLenum mode, glm::mat4 transform, Shader shader);
    void draw(glm::vec3 position, glm::quat rotation, glm::vec3 scale, Shader shader, Camera camera, glm::vec3 colour);
    void update_joints(Node *node);
    void update_animations(float delta_time, uint32_t animation_index);
    void load_material(tinygltf::Model &input);
    void load_node(const tinygltf::Node &input_node, tinygltf::Model &input, Node *parent, uint32_t node_index, std::vector<uint32_t> &index_buffer, std::vector<Vertex> &vertex_buffer);
    void load_skins(tinygltf::Model &input);
    void load_animations(tinygltf::Model &input);
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

struct Skybox
{
    GLuint ID;
    GLuint VAO;
    GLuint VBO;
    
    Model cube_mesh = Model("cube.gltf");
    std::array<std::string, 6> filenames = {
        "pos_x", "neg_x",
        "pos_y", "neg_y",
        "pos_z", "neg_z"
    };

    Skybox() {}; // default constructor.
    Skybox(int level_index);
    void draw(glm::vec3 position, Shader shader, Camera camera);
};