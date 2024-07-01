#include "draw.hpp"
#include "defines.hpp"                  // window dimensions.
#include <iostream>                     // std::cout etc.
#include <glm/gtc/type_ptr.hpp>         // get type of pointer for shaders.
#include <stb_image.h>                  // load images (include seperately from tinygltf).
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION        // stb image for textures.
#define STB_IMAGE_WRITE_IMPLEMENTATION  // stb image write for tinygltf.
#include <tiny_gltf.h>                  // actually include the file.

#define MODELS_PATH     "./assets/models/"
#define TEXTURES_PATH   "./assets/textures/"
using std::cout;

// link vertex attributes such as position, normals, and texcoords to VBO.
void link_attrib(GLuint layout, GLuint num_components, GLenum type, GLsizeiptr stride, void* offset, GLuint VBO)
{
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glEnableVertexAttribArray(layout);
    glVertexAttribPointer(layout, num_components, type, GL_FALSE, stride, offset);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

Mesh::Mesh(std::vector<Vertex> &vertices, std::vector<GLuint> &indices, std::vector<Texture> &textures)
{
    // set local mesh vectors to input for use in draw function.
    Mesh::vertices  = vertices;
	Mesh::indices   = indices;
    Mesh::textures  = textures;

    glGenVertexArrays(1, &VAO); // generate VAO.
    glBindVertexArray(VAO);     // bind VAO.

    // generate VBO, bind, then pass vertices vector.
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    // generate EBO, bind, then pass indices vector.
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    // link vertex attributes (position, normal, colour, texcoords)..
    link_attrib(0, 3, GL_FLOAT, sizeof(Vertex), (void*)(0), VBO);                   // position.
    link_attrib(1, 3, GL_FLOAT, sizeof(Vertex), (void*)(3 * sizeof(float)), VBO);   // normal.
    link_attrib(2, 3, GL_FLOAT, sizeof(Vertex), (void*)(6 * sizeof(float)), VBO);   // colour.
    link_attrib(3, 2, GL_FLOAT, sizeof(Vertex), (void*)(9 * sizeof(float)), VBO);   // texture.

    glBindVertexArray(0);                       // unbind VAO.
    glBindBuffer(GL_ARRAY_BUFFER, 0);           // unbind VBO.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);   // unbind EBO.
}

// draw the supplied mesh with given parameters.
void Mesh::draw(GLenum mode, glm::vec3 position, glm::quat rotation, glm::vec3 scale, Shader shader, glm::vec3 colour)
{
    glUseProgram(shader.ID);    // activate the shader being used to draw this model. (potential for multiple shaders per model?)
    glBindVertexArray(VAO);     // bind the VBO with the vertexes from the mesh.

    // (T*R*S) translation, rotation, and scale of mesh as a single matrix.
    glm::mat4 mvp = translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);

    
    // atm this loop does nothing as textures only ever has 1 element.
    // TODO: add multiple textures per mesh? atm each texture is rendered as seperate meshes, which seems ok?
    // for (size_t i = 0; i < textures.size(); i++)
    // {
    //     glActiveTexture(GL_TEXTURE0);
    //     glBindTexture(GL_TEXTURE_2D, textures[i].ID);
    //     glUniform1i(glGetUniformLocation(shader.ID, "tex0"), 0);
    // }

    // bind texture from mesh.
    if (!textures.empty())
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[0].ID);
        glUniform1i(glGetUniformLocation(shader.ID, "tex0"), 0);
    }

    // per mesh shader updates; colour and combined matrix transformation.
    glUniform3fv(glGetUniformLocation(shader.ID, "albedo"), 1, glm::value_ptr(colour));                     // base colour of mesh.
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "mvp"), 1, GL_FALSE, glm::value_ptr(mvp));           // mesh view projection matrix.
    
    // set polygon mode and then draw elements.
    glPolygonMode(GL_FRONT_AND_BACK, shader.mode);
    glLineWidth(1.0f);
    glDrawElements(mode, indices.size() * sizeof(indices[0]), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// change this tinygltf stuff to just json.hpp stuff cause i cba dealing with this lib anymore Lol.
// maybe lol. might be a lot of effort?
Texture::Texture(tinygltf::Image &image)
{
    name = image.name;
    std::cout << "texture: " << name << "\n";

    // generate texture using ID.
    glGenTextures(1, &ID);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ID);

    // texture settings.
    // glPixelStorei(GL_UNPACK_ALIGNMENT, 4);  // dunno what this does lol.
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // GL_LINEAR = bilinear filter.
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR); // GL_LINEAR_MIPMAP_LINEAR = trilinear filter.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // determine image format from number of components. 
    switch (image.component)
    {
    case 1:
        format = GL_RED;
        break;
    case 2:
        format = GL_RG;
        break;
    case 3:
        format = GL_RGB;
        break;
    default:
        format = GL_RGBA;
        break;
    }

    // determine image type from number of bits.
    switch (image.bits)
    {
    case 16:
        type = GL_UNSIGNED_SHORT;
        break;
    case 32:
        type = GL_UNSIGNED_SHORT;
        break;
    default:
        type = GL_UNSIGNED_BYTE;
        break;
    }

    // generate texture with parameters.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, format, type, image.image.data());
    glGenerateMipmap(GL_TEXTURE_2D);

    // delete texture! need to add the stbi clear thing here somehow?
    glBindTexture(GL_TEXTURE_2D, 0);
}

// for reading .gltf buffers.
// is a template as buffers store varying types of data but are accessed the same way.
template <typename T> T get_buffer(tinygltf::Accessor &accessor, tinygltf::Model &model)
{
    const tinygltf::BufferView& buffer_view = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer          = model.buffers[buffer_view.buffer];
    return reinterpret_cast<T>(&buffer.data[buffer_view.byteOffset + accessor.byteOffset]);
}

// recursive node traversal function to load from .gltf file.
// atm just loads mesh data but will add animation loading etc.
// a node can have mesh, armature, bone etc. 
void Model::load_from_node(tinygltf::Model &model, tinygltf::Node &node)
{
    cout << "current node: " << node.name << "\n";

    // get transforms of current node.
    glm::vec3 translation   = glm::vec3(0.0f);
    glm::quat rotation      = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale         = glm::vec3(1.0f);

    // get translation, rotation, scale, (and matrix?) if available here.
    if (node.translation.size() > 0)
    {
        translation = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
    }
    
    // gltf files store rotations in (x, y, z, w) order, glm quats are (w, x, y, z).
    if (node.rotation.size() > 0)
    {
        rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
    }

    // x, y, z scale.
    if (node.scale.size() > 0)
    {
        scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
    }

    // MESH LOAD.
    // if the current node has a mesh (other check is if mesh in question is part of the model but dunno if needed?)
    // node.mesh is the ("mesh" : 0) part of gltf file.
    if ((node.mesh >= 0) && ((size_t)node.mesh < model.meshes.size()))
    {
        // loop through each primitive in the mesh.
        // meshes usually just have one primitive though.
        for (size_t i = 0; i < model.meshes[node.mesh].primitives.size(); ++i)
        {
            // primitive shorthand to make stuff easier to read.
            tinygltf::Primitive &this_mesh = model.meshes[node.mesh].primitives[i];

            // create vectors for loaded data that will be used to create the mesh.
            std::vector<Vertex> vertices;
            std::vector<GLuint> indices;
            std::vector<Texture> textures;

            // accessors for each attribute of the mesh.
            tinygltf::Accessor position_acc = model.accessors[this_mesh.attributes["POSITION"]];
            tinygltf::Accessor normal_acc   = model.accessors[this_mesh.attributes["NORMAL"]];
            tinygltf::Accessor texcoord_acc = model.accessors[this_mesh.attributes["TEXCOORD_0"]];
            tinygltf::Accessor indices_acc  = model.accessors[this_mesh.indices];

            // add stuff here so that it determines the type needed automatically. ie short etc for indices changes.
            // could add checks to see if undefined etc but not needed rlly atm.
            const float* positions_buffer           = get_buffer<const float*>(position_acc, model);
            const float* normals_buffer             = get_buffer<const float*>(normal_acc, model);
            const float* texcoords_buffer           = get_buffer<const float*>(texcoord_acc, model);
            const unsigned short* indices_buffer    = get_buffer<const unsigned short*>(indices_acc, model);

            // get vertex data from buffer and push to vertices vector.
            for (size_t i = 0; i < position_acc.count; ++i)
            {
                Vertex vertex;
                vertex.position = rotation * (glm::vec3(positions_buffer[i * 3 + 0],
                                                        positions_buffer[i * 3 + 1],
                                                        positions_buffer[i * 3 + 2]) * scale) + translation;
                                        
                vertex.normal   = rotation * glm::vec3( normals_buffer[i * 3 + 0],
                                                        normals_buffer[i * 3 + 1],
                                                        normals_buffer[i * 3 + 2]);

                vertex.color    = vertex.normal;
                vertex.texUV    = glm::vec2(texcoords_buffer[i * 2 + 0], texcoords_buffer[i * 2 + 1]);
                vertices.push_back(vertex);
            }

            // get indices from buffer and push to indices vector.
            for (size_t i = 0; i < indices_acc.count; ++i)
            {
                indices.push_back((GLuint)indices_buffer[i]);
            }

            // if (model.textures.size() > 0)
            // check if the current model has any textures associated with it.
            // don't think this works properly atm.
            if (this_mesh.material >= 0)
            {
                // create a texture pointer equal to the texture associated with the current mesh's material.
                int texture_ID = model.textures[this_mesh.material].source;

                // if texture source exists, push texture source image as a new texture.
                if (texture_ID >= 0)
                {
                    bool is_duplicate = false;
                    for (size_t j = 0; j < loaded_textures.size(); j++)
                    {
                        if (loaded_textures[j].name == model.images[texture_ID].name)
                        {
                            textures.push_back(loaded_textures[j]);
                            is_duplicate = true;
                            break;
                        }
                    }

                    // texture is not in loaded textures array, so add to both it and the textures array.
                    if (!is_duplicate)
                    {
                        loaded_textures.push_back(Texture(model.images[texture_ID]));
                        textures.push_back(loaded_textures.back());
                    }                    
                }
            }
            else
            {
                cout << "NO TEXTURE AT current node: " << node.name << "\n";
                // should eventually push back a null/empty texture here rlly?
                // figure out how to do that without using like a blank png lmao.
            }

            // finally, create a new mesh object using the above data and push to model meshes vector.
            Mesh mesh = Mesh(vertices, indices, textures);
            
            // if flag added in blender, it does not collide.
            if (model.meshes[node.mesh].extras.IsObject())
            {
                mesh.collider = false;
            }
            else
            {
                mesh.collider = true;
            }
            
            meshes.push_back(mesh);
        }
    }

    // load armature(s) here i think...
    // i think the check here is if it has any children? cause mb only armatures have children field.


    // recursively traverse each node in the scene.
    for (size_t i = 0; i < node.children.size(); i++)
    {
        load_from_node(model, model.nodes[node.children[i]]);
    }
}

// load a model from a .gltf file.
Model::Model(std::string filename)
{
    tinygltf::Model model;      // stores .gltf model reference.
    tinygltf::TinyGLTF loader;  // stores ASCII from file.
    std::string err;            // outputs warning if fails to load properly.
    std::string warn;           // outputs error if any errors.

    bool loaded = loader.LoadASCIIFromFile(&model, &err, &warn, MODELS_PATH + filename);
    if (!warn.empty())  { cout << "WARN: " << warn << "\n"; }
    if (!err.empty())   { cout << "ERR: " << err << "\n"; }

    // if loaded correctly, load file contents.
    if (loaded)
    {
        cout << "model: " << filename << "\n";

        // recursively traverse each node in the scene.
        const tinygltf::Scene &scene = model.scenes[model.defaultScene];
        for (size_t i = 0; i < scene.nodes.size(); ++i)
        {
            load_from_node(model, model.nodes[scene.nodes[i]]);
        }

        cout << "\n";
    }
}

// draw model by drawing each mesh contained within the model.
void Model::draw(glm::vec3 position, glm::quat rotation, glm::vec3 scale, Shader shader, glm::vec3 colour)
{
    for (size_t i = 0; i < meshes.size(); i++)
	{
        meshes[i].draw(GL_TRIANGLES, position, rotation, scale, shader, colour);
	}
}

// grid
Grid::Grid(int slices)
{
    // set grid object slices to input.
    Grid::slices = slices;

    // generate vertices from slice number.
    for(int i = 0; i <= slices; ++i)
    {
        for(int j = 0; j <= slices; ++j)
        {
            Vertex vertex;
            vertex.position = glm::vec3((float)j/(float)slices, 0.0f, (float)i/(float)slices);
            vertex.normal   = vertex.position;

            // draws the x and z axis in seperate colours.
            if ((i == slices/2) || (j == slices/2))
            {
                vertex.color = glm::vec3(0.0f);
            }
            else
            {
                vertex.color = glm::vec3(0.7f);
            }
            
            vertex.texUV    = glm::vec2(0.0f, 0.0f);
            vertices.push_back(vertex);
        }
    }

    // generate indices (in groups of 4).
    for(int i = 0; i < slices; ++i)
    {
        for(int j = 0; j < slices; ++j)
        {
            int row1 = (slices + 1) * (i);
            int row2 = (slices + 1) * (i +1);

            indices.push_back(row1 + j);
            indices.push_back(row1 + j + 1);
            indices.push_back(row1 + j + 1);
            indices.push_back(row2 + j + 1);
            indices.push_back(row2 + j + 1);
            indices.push_back(row2 + j);
            indices.push_back(row2 + j);
            indices.push_back(row1 + j);
        }
    }

    // update grid mesh with new vertices and indices.
    mesh = Mesh(vertices, indices, textures);
}

// draws grid at centre of world.
void Grid::draw(Shader shader)
{
    int sca             = 1;
    float xz_coord      = (float)((slices * -sca) / 2);
    glm::vec3 position  = glm::vec3(xz_coord, 0.0f, xz_coord);
    glm::quat rotation  = glm::quat(glm::vec3(0.0f));
    glm::vec3 scale     = glm::vec3((float)(slices * sca));

    // draw mesh with GL_LINES.
    mesh.draw(GL_LINES, position, rotation, scale, shader, glm::vec3(0.0f));
}

// grid
Frustum::Frustum(std::vector<glm::vec4> corners)
{
    // set grid object slices to input.
    // Vertex vertex;
    // vertex.color    = glm::vec3(1.0f);

    // // near face?
    // vertex.position = glm::vec3(-1.0f, -1.0f, -1.0f) * 5.0f;
    // vertices.push_back(vertex);
    // vertex.position = glm::vec3( 1.0f, -1.0f, -1.0f) * 5.0f;
    // vertices.push_back(vertex);
    // vertex.position = glm::vec3( 1.0f,  1.0f, -1.0f) * 5.0f;
    // vertices.push_back(vertex);
    // vertex.position = glm::vec3(-1.0f,  1.0f, -1.0f) * 5.0f;
    // vertices.push_back(vertex);

    // // far face?
    // vertex.position = glm::vec3(-1.0f, -1.0f,  1.0f) * 15.0f;
    // vertices.push_back(vertex);
    // vertex.position = glm::vec3( 1.0f, -1.0f,  1.0f) * 15.0f;
    // vertices.push_back(vertex);
    // vertex.position = glm::vec3( 1.0f,  1.0f,  1.0f) * 15.0f;
    // vertices.push_back(vertex);
    // vertex.position = glm::vec3(-1.0f,  1.0f,  1.0f) * 15.0f;
    // vertices.push_back(vertex);


    // for (size_t i = 0; i < 8; i++)
    // {
    //     Vertex vertex;
    //     vertex.color    = glm::vec3(1.0f);
    //     vertex.position = corners[i];
    //     vertices.push_back(vertex);
    // }


    Vertex vertex;
    vertex.color    = glm::vec3(1.0f);

    // near face?
    vertex.position = corners[0];
    vertices.push_back(vertex);
    vertex.position = corners[1];
    vertices.push_back(vertex);
    vertex.position = corners[2];
    vertices.push_back(vertex);
    vertex.position = corners[3];
    vertices.push_back(vertex);

    // far face?
    vertex.position = corners[4];
    vertices.push_back(vertex);
    vertex.position = corners[5];
    vertices.push_back(vertex);
    vertex.position = corners[6];
    vertices.push_back(vertex);
    vertex.position = corners[7];
    vertices.push_back(vertex);


    indices = {
        0, 1, 3, 3, 1, 2, // face
        1, 5, 2, 2, 5, 6,
        5, 4, 6, 6, 4, 7,
        4, 0, 7, 7, 0, 3,

        3, 2, 7, 7, 2, 6,

        3, 7, 3, 3, 7, 6
    };


    // update grid mesh with new vertices and indices.
    mesh = Mesh(vertices, indices, textures);
}

// draws grid at centre of world.
void Frustum::draw(glm::vec3 position, Shader shader)
{
    glm::quat rotation  = glm::quat(glm::vec3(0.0f));
    glm::vec3 scale     = glm::vec3(1.0f);

    // draw mesh with GL_LINES.
    mesh.draw(GL_LINES, position, rotation, scale, shader, glm::vec3(2.0f));
}

// very simple circle for collision shapes. 
Circle::Circle(float radius)
{
    // make vertices along a circle. increment = resolution?
    for (float i = 0; i < 360; i++)
    {
        Vertex vertex;
        vertex.position = glm::vec3(sinf(glm::radians(i)) * radius, 0.0f, cosf(glm::radians(i)) * radius);
        vertex.normal   = vertex.position;
        vertex.color    = glm::vec3(0.0f);
        vertex.texUV    = glm::vec2(0.0f, 0.0f);

        vertices.push_back(vertex);
        indices.push_back(i);
    }

    // update grid mesh with new vertices and indices.
    mesh = Mesh(vertices, indices, textures);
}

// draw circle.
void Circle::draw(glm::vec3 position, Shader shader, glm::vec3 colour)
{
    glm::quat rotation  = glm::quat(glm::vec3(0.0f));
    glm::vec3 scale     = glm::vec3(1.0f);

    // draw mesh with GL_LINES.
    mesh.draw(GL_LINE_LOOP, position, rotation, scale, shader, colour);
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
    link_attrib(0, 2, GL_FLOAT, 4 * sizeof(float), (void*)(0), VBO);
    link_attrib(1, 2, GL_FLOAT, 4 * sizeof(float), (void*)(2 * sizeof(float)), VBO);
    
    // color texture attachment.
    glGenTextures(1, &ID);
    glBindTexture(GL_TEXTURE_2D, ID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // render buffer object.
    glGenRenderbuffers(1, &RBO);
    glBindRenderbuffer(GL_RENDERBUFFER, RBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, WINDOW_WIDTH, WINDOW_HEIGHT);

    // attach texture and RBO to FBO.
    glGenFramebuffers(1, &FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ID, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);

    // check if any errors with framebuffer.
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "FRAMEBUFFER NOT COMPLETE!" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// draw framebuffer texture to screen.
void ScreenTexture::draw(Shader &shader)
{
    // clear buffers etc before drawing.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    // glClear(GL_COLOR_BUFFER_BIT);
    
    // apply shader and bind texture.
    glUseProgram(shader.ID);
    glBindVertexArray(VAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ID);

    // send gamma to post process shader.
    glUniform1f(glGetUniformLocation(shader.ID, "gamma"), gamma);

    // draw the framebuffer.
    glPolygonMode(GL_FRONT_AND_BACK, shader.mode);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(0);
}

// drawing image to UI etc.
Image2D::Image2D(std::string filename)
{
    unsigned char *data = stbi_load((TEXTURES_PATH + filename).data(), &width, &height, &component, STBI_rgb_alpha);
    if(!data)
    {
        throw(std::string("Failed to load texture"));
    }

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
    link_attrib(0, 2, GL_FLOAT, 4 * sizeof(float), (void*)(0), VBO);
    link_attrib(1, 2, GL_FLOAT, 4 * sizeof(float), (void*)(2 * sizeof(float)), VBO);

    // generate texture with parameters.
    glGenTextures(1, &ID);
    glBindTexture(GL_TEXTURE_2D, ID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // delete texture!
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
}

void Image2D::draw(float x, float y, float scale)
{
    glUseProgram(shader.ID);
    glBindVertexArray(VAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ID);

    // send all parameters to shader uniforms.
    glUniform1f(glGetUniformLocation(shader.ID, "x_pos"), x);
    glUniform1f(glGetUniformLocation(shader.ID, "y_pos"), y);
    glUniform1f(glGetUniformLocation(shader.ID, "scale"), scale);
    glUniform1f(glGetUniformLocation(shader.ID, "window_width"), (float)WINDOW_WIDTH);
    glUniform1f(glGetUniformLocation(shader.ID, "window_height"), (float)WINDOW_HEIGHT);
    glUniform1f(glGetUniformLocation(shader.ID, "img_width"), width);
    glUniform1f(glGetUniformLocation(shader.ID, "img_height"), height);
    
    // enable alpha blending for transparency.
    glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // draw image quad.
    glPolygonMode(GL_FRONT_AND_BACK, shader.mode);
	glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisable(GL_BLEND);    // turn off blending.
	glBindVertexArray(0);   // unbind VAO.
}