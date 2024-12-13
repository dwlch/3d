#pragma once

#include <tiny_gltf.h>  // gltf loader.
#include <string>       // for model filename.
#include <vector>       // for vertices, indices, and textures arrays.
#include <array>
#include <json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "shader.hpp"   // for updating shader uniforms per mesh.

#define MAX_JOINTS 100

// store vertex information from gltf file.
struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 texUV;
    glm::vec4 joint_indices;    // vertex has at most 4 joint IDs it is connected to.
    glm::vec4 joint_weights;    // for each joint, there is an associatd weight value.
};

// referenced: https://github.com/SaschaWillems/Vulkan/blob/master/examples/gltfskinning/gltfskinning.cpp
// kinda acts more like a material struct rn.
// this really only needs to be an ID value honestly lol.
struct Texture
{
    GLuint ID;
    GLenum format;
    GLenum type;
    std::string name;

    Texture(std::string &filename, int &width, int &height, int &component, int &bits, unsigned char* image_data);
};

struct Material
{
    glm::vec4 base_colour = glm::vec4(1.0f);
    uint32_t base_colour_texture_index;
};

struct MeshPrimitive
{
    GLuint VAO; // vertex array object.
    GLuint VBO; // vertex buffer object array.
    GLuint EBO; // element buffer object.

    uint32_t first_index;
    uint32_t index_count;
    int32_t material_index; // index of mesh's material.

    // i actually think we want a vertex buffer per node, not model, otherwise the collision wont work.
    // oh or could just have a dedicated mesh_collider_vertices vector lol.
    std::vector<uint32_t> index_buffer;
    std::vector<Vertex> vertex_buffer;
    std::vector<Vertex> vertex_collider_buffer;
};

// decribes a single node in the gltf file.
struct Node; // recursive?
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
    std::array<glm::mat4, MAX_JOINTS>   joint_matrix;
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
};

// deprecating this rn, is just being used for collider loading atm.
struct Mesh
{
    std::vector<Vertex> vertices;       // store vertices publicly for collision load?
    std::vector<GLuint> indices;        // indice for each face.


    GLuint VAO; // vertex array object.
    GLuint VBO; // vertex buffer object array.
    GLuint EBO; // element buffer object.
    int type;   // 0 = solid, 1 = passable, 2 = trigger.

    Mesh();
    Mesh(std::vector<Vertex> &vertices, std::vector<GLuint> &indices, int type);
};

// model class which contains a number of meshes which are drawn individually.
struct Model
{
    std::vector<Mesh>       meshes;     // store each mesh in the .gltf file seperately as a mesh object.
    std::vector<Node *>     nodes;      // store all nodes. replacing meshes with nodes atm.
    std::vector<Texture>    textures;   // load each texture only once and copy from here if duplicate. per model.
    std::vector<Material>   materials;
    std::vector<Skin>       skins;  // armature per mesh? i think that's how it works.
    std::vector<Animation>  animations;

    uint32_t active_animation = 0;

    Model();
    Model(std::string filename);
    Node *find_node(Node *parent, uint32_t index);
    Node *node_from_index(uint32_t index);
    void bind_node(Node *node);
    void draw_node(Node node, GLenum mode, glm::mat4 transform, Shader shader);
    void draw(glm::vec3 position, glm::quat rotation, glm::vec3 scale, Shader shader, glm::vec3 colour);
    void update_joints(Node *node);
    void update_animations(float delta_time);
    void load_material(tinygltf::Model &input);
    void load_node(const tinygltf::Node &input_node, tinygltf::Model &input, Node *parent, uint32_t node_index, std::vector<uint32_t> &index_buffer, std::vector<Vertex> &vertex_buffer);
    void load_skins(tinygltf::Model &input);
    void load_animations(tinygltf::Model &input);
};

// a type of mesh. procedural grid generated based on number of given slices.
class Grid
{
    public:
        Grid(int slices);
        void draw(Shader shader);

    private:
        int slices;                                     // number of grid slices.
        std::vector<Vertex> vertices;                   // grid vertices stored here.
        std::vector<GLuint> indices;                    // grid indices.
        Mesh mesh = Mesh(vertices, indices, 1);  // mesh to store above in.
};

// a type of mesh. procedural grid generated based on number of given slices.
class Frustum
{
    public:
        Frustum(std::vector<glm::vec4> corners);
        void draw(glm::vec3 position, Shader shader);

    private:
        std::vector<Vertex> vertices;                   // grid vertices stored here.
        std::vector<GLuint> indices;                    // grid indices.
        Mesh mesh = Mesh(vertices, indices, 1);  // mesh to store above in.
};

// a type of mesh. procedural grid generated based on number of given slices.
class Circle
{
    public:
        Circle(float radius);
        void draw(glm::vec3 position, Shader shader, glm::vec3 colour);

    private:
        GLuint VAO; // vertex array object.
        GLuint VBO; // vertex buffer object array.
        GLuint EBO; // element buffer object.
        std::vector<Vertex> vertices;                   // grid vertices stored here.
        std::vector<GLuint> indices;                    // grid indices.
};



// basically a quad that draws the scene w/ post-processing added.
struct ScreenTexture
{
    GLuint ID;
    GLuint VAO;
    GLuint VBO;
    GLuint RBO;
    GLuint FBO;
    float gamma = 0.9f;

    ScreenTexture();
    void draw(Shader &shader);
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