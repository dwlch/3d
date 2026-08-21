#pragma once

#include <tiny_gltf.h>  // gltf loader.
#include <string>       // for model filename.
#include <vector>
#include <array>
#include <json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>

#include "shader.hpp"   // for updating shader uniforms per mesh.
#include "camera.hpp"   // draw functions.

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
    GLuint VAO; // vertex array object.
    GLuint VBO; // vertex buffer object array.
    GLuint EBO; // element buffer object.

    uint32_t first_index;   // first index of mesh within vector of indices *UNUSED RN*
    uint32_t index_count;   // number of indices in the mesh.
    int32_t material_index; // index of the mesh's material in the model's materials array.

    std::vector<uint32_t> index_buffer;         // stores the list of indices.
    std::vector<Vertex> vertex_buffer;          // stores the raw vertices.
    
};

struct Node; // so nodes can have parent nodes.
struct Node
{
    Node *                      parent;
    uint32_t                    index;
    int32_t                     skin = -1;
    std::vector<Node *>         children;
    std::vector<MeshPrimitive>  mesh_primitives;
    std::vector<glm::vec3>      collision_vertices;  // stores collision data of mesh.

    glm::vec3                   translation{};
    glm::quat                   rotation{};
    glm::vec3                   scale   = glm::vec3(1.0f);
    glm::mat4                   matrix  = glm::mat4(1.0f);
    glm::mat4                   get_local_matrix();


    // maybe the solution is to load the extras into the node itself?
    // that kinda makes sense.
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

// loads from .gltf/.glb file. used for level structure as well as models.
struct glTF
{
    std::vector<Node *> nodes;          // a node has a mesh and inheritence hierarchy.
    std::vector<Material> materials;    // materials (colour, texture, can add more stuff like normals).

    glTF() {};
    Node *find_node(Node *parent, uint32_t index);
    Node *node_from_index(uint32_t index);
    void bind_node(Node *node);
    void draw_node(Node node, GLenum mode, glm::mat4 transform, Shader shader);
    void draw(glm::vec3 position, glm::quat rotation, glm::vec3 scale, Shader shader, Camera camera, glm::vec3 colour);
    void load_material(tinygltf::Model &input);
    void load_node(const tinygltf::Node &input_node, tinygltf::Model &input, Node *parent, uint32_t node_index, std::vector<uint32_t> &index_buffer, std::vector<Vertex> &vertex_buffer);
    void load_from_file(std::string filename);
    
    // animation.
    std::vector<Skin>       skins;      // armature per mesh? i think that's how it works.
    std::vector<Animation>  animations; // each animation accessed by the index.
    uint32_t active_animation = 0;      // general format: 0 = idle, 1 = walk, 2 = jump/fall.

    void load_skins(tinygltf::Model &input);
    void load_animations(tinygltf::Model &input);
    void update_joints(Node *node);
    void update_animations(float delta_time, uint32_t animation_index);

    // models and levels need different extras.
    virtual void get_extras(const tinygltf::Node *in_node, Node *out_node) = 0;


    // actually this should be a templated thing that you give a string (name of the property) and a type, and it returns the value!
    template <typename T> T get_node_extras(const tinygltf::Value *extras, std::string property_name);
};

// atm this is just an alias for the parent struct -- doesn't really do anything rn except be a nicer name.
struct Model : public glTF
{
    Model() {}
    void get_extras(const tinygltf::Node *in_node, Node *out_node) override {};
};

void link_attrib(GLuint VBO, GLuint layout, GLuint size, GLenum type, GLsizeiptr stride, void *offset);
void bind_buffers(GLuint &VAO, GLuint &VBO, GLuint &EBO, std::vector<Vertex> vertices, std::vector<GLuint> indices);