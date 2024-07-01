#pragma once

#include <tiny_gltf.h>  // gltf loader.
#include <string>       // for model filename.
#include <vector>       // for vertices, indices, and textures arrays.
#include <json.hpp>
#include <glm/glm.hpp>

#include "shader.hpp"   // for updating shader uniforms per mesh.

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 texUV;
};

// decribes a single joint in the armature (ie head, leg etc)
struct Joint
{
    int ID;
};

// each armature is a collection of (ordered??) joints.
struct Armature
{
    std::vector<Joint> joints;
};

// kinda acts more like a material struct rn.
struct Texture
{
    GLuint ID;
    GLenum format;
    GLenum type;
    std::string name;

    Texture(tinygltf::Image &image);
};

// meshes contain vertices and indices from a source.
// mesh can be part of a model (array of meshes) or grid (a single mesh) atm.
struct Mesh
{
    std::vector<Vertex> vertices;       // store vertices publicly for collision load?
    std::vector<GLuint> indices;        // indice for each face.
    std::vector<Texture> textures;

    GLuint VAO; // vertex array object.
    GLuint VBO; // vertex buffer object array.
    GLuint EBO; // element buffer object.
    bool collider = false;              // set if want to be solid. change to enum so can have collide/trigger/no collision.

    Mesh();
    Mesh(std::vector<Vertex> &vertices, std::vector<GLuint> &indices, std::vector<Texture> &textures);
    void draw(GLenum mode, glm::vec3 position, glm::quat rotation, glm::vec3 scale, Shader shader, glm::vec3 colour);
};

// model class which contains a number of meshes which are drawn individually.
struct Model
{
    std::vector<Mesh> meshes;               // store each mesh in the .gltf file seperately as a mesh object.
    std::vector<Armature> armatures;        // armature per mesh? i think that's how it works.
    std::vector<Texture> loaded_textures;   // load each texture only once and copy from here if duplicate.

    // create mesh from .gltf file, generate VBO and EBO using .gltf vertices and indices.
    Model(std::string filename);
    void draw(glm::vec3 position, glm::quat rotation, glm::vec3 scale, Shader shader, glm::vec3 colour);
    void load_from_node(tinygltf::Model &model, tinygltf::Node &node);
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
        std::vector<Texture> textures;                  // empty as grid has no textures? (why is this here)
        Mesh mesh = Mesh(vertices, indices, textures);  // mesh to store above in.
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
        std::vector<Texture> textures;                  // empty as grid has no textures? (why is this here)
        Mesh mesh = Mesh(vertices, indices, textures);  // mesh to store above in.
};

// a type of mesh. procedural grid generated based on number of given slices.
class Circle
{
    public:
        Circle(float radius);
        void draw(glm::vec3 position, Shader shader, glm::vec3 colour);

    private:
        std::vector<Vertex> vertices;                   // grid vertices stored here.
        std::vector<GLuint> indices;                    // grid indices.
        std::vector<Texture> textures;                  // empty as grid has no textures? (why is this here)
        Mesh mesh = Mesh(vertices, indices, textures);  // mesh to store above in.
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