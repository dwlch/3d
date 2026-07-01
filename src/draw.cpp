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
#define ERROR_PNG       "error.png"
using std::cout;

glm::mat4 Node::get_local_matrix()
{
	return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
}

glm::mat4 get_node_matrix(Node *node)
{
	glm::mat4 node_matrix   = node->get_local_matrix();
	Node *current_parent    = node->parent;

	while (current_parent)
	{
		node_matrix    = current_parent->get_local_matrix() * node_matrix;
		current_parent = current_parent->parent;
	}
	return node_matrix;
}

void Model::draw_node(Node node, GLenum mode, glm::mat4 transform, Shader shader)
{
    // draw mesh of node.
    if (node.mesh_primitives.size() > 0)
    {
        // node matrix combined with model transform.
        glm::mat4 node_transform = transform * get_node_matrix(&node);

        // loop through each mesh in the node (usually just one atm).
        for (MeshPrimitive &mesh : node.mesh_primitives)
        {
            // very hacky fix rn to make triggers not cast shadows -- fix later.
                // if (!(mesh.type == 1 && shader.ID == 6)) // ID 6 is shadowmap.
                // {
                if (mesh.index_count > 0)
                {
                    // if (mesh.is_trigger)
                    // {
                    //     shader.mode = GL_LINE;
                    // }
                    // bind the VAO with the vertexes from the mesh.
                    glBindVertexArray(mesh.VAO);    
                    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "mvp"), 1, GL_FALSE, glm::value_ptr(node_transform));

                    // if mesh has a material/texture attached to it.
                    if (mesh.material_index > -1)
                    {
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, materials[mesh.material_index].texture_ID);
                        glUniform1i(glGetUniformLocation(shader.ID, "tex0"), 0);
                    }

                    // maybe rather than shader.mode, set when loading trigger vs mesh?
                    // set polygon mode and then draw elements.
                    glPolygonMode(GL_FRONT_AND_BACK, shader.mode);

                    // switch (mesh.type)
                    // {
                    // case 0:
                    //     glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                    //     break;
                    // case 1:
                    //     glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    //     break;
                    
                    // default:
                    //     break;
                    // }



                    glLineWidth(1.0f);
                    glDrawElements(mode, mesh.index_buffer.size() * sizeof(mesh.index_buffer[0]), GL_UNSIGNED_INT, 0);

                    // unbind vertex array and texture.
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glBindVertexArray(0);
                }
            // }
        }
    }

    // // draw bone of node.
    // if (node.skin > -1)
    // {
    //     Line bone_mesh(node.translation, 2.0f);
    //     bone_mesh.draw(node.translation, transform * get_node_matrix(&node), shader, glm::vec3(1.0f, 1.0f, 1.0f));


    // }

    for (auto &child : node.children)
    {
        draw_node(*child, mode, transform, shader);
    }
}

// draw model by drawing each mesh contained within the model.
void Model::draw(glm::vec3 position, glm::quat rotation, glm::vec3 scale, Shader shader, Camera camera, glm::vec3 colour)
{
    glm::mat4 transform = translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);

    // per-model uniforms.
    glUseProgram(shader.ID);
    glUniform1f(glGetUniformLocation(shader.ID, "camera_distance"), camera.distance_offset);
    glUniform1fv(glGetUniformLocation(shader.ID, "cascade_bounds"), NUM_CASCADES, reinterpret_cast<GLfloat *>(camera.cascade_bounds.data()));
    glUniform3fv(glGetUniformLocation(shader.ID, "albedo"), 1, glm::value_ptr(colour));
    glUniform3fv(glGetUniformLocation(shader.ID, "light_pos"), 1, glm::value_ptr(camera.light_pos));
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "view"), 1, GL_FALSE, glm::value_ptr(camera.mvp));
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "light"), NUM_CASCADES, GL_FALSE, reinterpret_cast<GLfloat *>(camera.cascade_proj.data()));
    
    for (int i = 0; i < NUM_CASCADES; ++i)
    { 
        // bind from texture1 onwards. texture0 is for mesh textures atm.
        glActiveTexture(GL_TEXTURE1 + i);
        glBindTexture(GL_TEXTURE_2D, camera.depth_maps[i]);
        glUniform1i(glGetUniformLocation(shader.ID, std::string("shadow_map[" + std::to_string(i) + "]").c_str()), i + 1);
           
    }

    for (auto skin : skins)
    {
        // glUniformMatrix4fv(glGetUniformLocation(shader.ID, "joint_matrices"), MAX_JOINTS, GL_FALSE, glm::value_ptr(skin.joint_matrix[0]));
        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "joint_matrices"), MAX_JOINTS, GL_FALSE, reinterpret_cast<GLfloat *>(skin.joint_matrix.data()));
    }

    // loop through all nodes in the model.
    for (auto &node : nodes)
    {
        draw_node(*node, GL_TRIANGLES, transform, shader);
    }
}

template <typename T> const T *get_buffer(tinygltf::Model &model, tinygltf::Primitive primitive, std::string name, int type, int &stride)
{
    const T *buffer = nullptr;
    if (primitive.attributes.find(name) != primitive.attributes.end())
    {
        const tinygltf::Accessor &acc       = model.accessors[primitive.attributes.find(name)->second];
        const tinygltf::BufferView &view    = model.bufferViews[acc.bufferView];
        buffer                              = reinterpret_cast<const T *>(&model.buffers[view.buffer].data[view.byteOffset + acc.byteOffset]);
        stride                              = acc.ByteStride(view) ? (acc.ByteStride(view) / tinygltf::GetComponentSizeInBytes(acc.componentType)) : tinygltf::GetNumComponentsInType(type);
    }
    return buffer;
}

void Model::load_node(const tinygltf::Node &input_node, tinygltf::Model &input, Node *parent, uint32_t node_index, std::vector<uint32_t> &index_buffer, std::vector<Vertex> &vertex_buffer)
{
    Node *node      = new Node{};
    node->parent    = parent;
    node->index     = node_index;
    node->skin      = input_node.skin;
    node->matrix    = glm::mat4(1.0f);

    // get the node's transform, either as in T*R*S format, or a matrix.
    if (!input_node.translation.empty())
    {
        node->translation = glm::make_vec3(input_node.translation.data());
    }
    if (!input_node.rotation.empty())
    {
        node->rotation = glm::mat4(glm::quat(glm::make_quat(input_node.rotation.data())));
    }
    if (!input_node.scale.empty())
    {
        node->scale = glm::make_vec3(input_node.scale.data());
    }
    if (!input_node.matrix.empty())
    {
        node->matrix = glm::make_mat4x4(input_node.matrix.data());
    }

    // load children of node.
    if (input_node.children.size() > 0)
    {
        for (size_t i = 0; i < input_node.children.size(); ++i)
        {
            load_node(input.nodes[input_node.children[i]], input, node, input_node.children[i], index_buffer, vertex_buffer);
        }
    }

    // load mesh from node if available.
    if (input_node.mesh > -1)
    {
        const tinygltf::Mesh mesh = input.meshes[input_node.mesh];
        // cout << "Mesh: " << mesh.name << "\n";

        // loop through each primitive of the mesh (usually 1 per mesh):
        for (size_t i = 0; i < mesh.primitives.size(); ++i)
        {
            // where what is loaded is stored.
            MeshPrimitive this_mesh{};
            const tinygltf::Primitive &gltf_primitive = mesh.primitives[i];
            uint32_t first_index    = static_cast<uint32_t>(index_buffer.size());
            uint32_t vertex_start   = static_cast<uint32_t>(vertex_buffer.size());
            
            // byte strides/lengths for each buffer entry.
            int position_stride     = 0;
            int normal_stride       = 0;
            int texcoord_stride     = 0;
            int colours_stride      = 0;
            int joints_stride       = 0;
            int weights_stride      = 0;

            // retrieve all buffers.
            const auto positions_buffer     = get_buffer<float> (input, gltf_primitive, "POSITION",     TINYGLTF_TYPE_VEC3, position_stride);
            const auto normals_buffer       = get_buffer<float> (input, gltf_primitive, "NORMAL",       TINYGLTF_TYPE_VEC3, normal_stride);
            const auto texcoords_buffer     = get_buffer<float> (input, gltf_primitive, "TEXCOORD_0",   TINYGLTF_TYPE_VEC2, texcoord_stride);
            const auto colours_buffer       = get_buffer<float> (input, gltf_primitive, "COLOR_0",      TINYGLTF_TYPE_VEC4, colours_stride);
            const auto joint_indices_buffer = get_buffer<void>  (input, gltf_primitive, "JOINTS_0",     TINYGLTF_TYPE_VEC4, joints_stride);
            const auto joint_weights_buffer = get_buffer<float> (input, gltf_primitive, "WEIGHTS_0",    TINYGLTF_TYPE_VEC4, weights_stride);

            // get some specific things from the accessors.
            int joint_component_type    = input.accessors[gltf_primitive.attributes.find("JOINTS_0")->second].componentType;
            size_t vertices_count       = input.accessors[gltf_primitive.attributes.find("POSITION")->second].count;
            bool is_skinned             = joint_indices_buffer && joint_weights_buffer;

            // vertices.
            for (size_t j = 0; j < vertices_count; ++j)
            {
                Vertex vertex{};
                vertex.position = glm::make_vec3(&positions_buffer[j * position_stride]);
                vertex.normal   = normals_buffer    ? glm::normalize(glm::make_vec3(&normals_buffer[j * normal_stride])) : glm::vec3(0.0f);
                vertex.texUV    = texcoords_buffer  ? glm::make_vec2(&texcoords_buffer[j * texcoord_stride])                        : glm::vec2(0.0f);
                vertex.color    = colours_buffer    ? glm::vec3(glm::make_vec4(&colours_buffer[j * colours_stride]))                : glm::vec3(1.0f);
                // vertex.color    = vertex.normal;

                if (is_skinned)
                {
                    // joints.
                    switch (joint_component_type)
                    {
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        {
                            vertex.joint_indices = glm::vec4(glm::make_vec4(&reinterpret_cast<const uint8_t*>(joint_indices_buffer)[j * joints_stride]));
                            break;
                        }
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        {
                            vertex.joint_indices = glm::vec4(glm::make_vec4(&reinterpret_cast<const uint16_t*>(joint_indices_buffer)[j * joints_stride]));
                            break;
                        }
                        default:
                        {
                            cout << "Joint component type not supported.\n"; 
                            break;
                        }
                    }
                    // weights.
                    vertex.joint_weights = glm::vec4(glm::make_vec4(&joint_weights_buffer[j * weights_stride]));
                }
                else
                {
                    // no skin: assign default values to joints and weights.
                    vertex.joint_indices = glm::vec4(0.0f);
                    vertex.joint_weights = glm::vec4(0.0f);
                }

                // fix all zero weights (unsure why this is necessary vs defaulting to vec4(1.0f)).
                if (glm::length(vertex.joint_weights) == 0.0f)
                {
                    vertex.joint_weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                }
                
                // push back vector and use mesh vertices as collision shape.
                vertex_buffer.push_back(vertex);
                this_mesh.vertex_buffer.push_back(vertex);

                // for collider, apply mvp to vertex position exactly like in shader.
                // this feels like it could be optimised somehow.
                this_mesh.vertex_collider_buffer.push_back(glm::vec3(get_node_matrix(node) * glm::vec4(vertex.position, 1.0f)));
            }

            // indices.
            const tinygltf::Accessor &acc           = input.accessors[gltf_primitive.indices];
            const tinygltf::BufferView &view        = input.bufferViews[acc.bufferView];
            const tinygltf::Buffer &indices_buffer  = input.buffers[view.buffer];

            switch (acc.componentType)
            {
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                {
                    const uint32_t *buffer = reinterpret_cast<const uint32_t*>(&indices_buffer.data[acc.byteOffset + view.byteOffset]);
                    for (size_t j = 0; j < acc.count; ++j)
                    {
                        index_buffer.push_back(buffer[j] + vertex_start);
                        this_mesh.index_buffer.push_back(buffer[j]);
                    }
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: // 5123, mostly going here i think.
                {
                    const uint16_t *buffer = reinterpret_cast<const uint16_t*>(&indices_buffer.data[acc.byteOffset + view.byteOffset]);
                    for (size_t j = 0; j < acc.count; ++j)
                    {
                        index_buffer.push_back(buffer[j] + vertex_start);
                        this_mesh.index_buffer.push_back(buffer[j]);
                    }
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                {
                    const uint8_t *buffer = reinterpret_cast<const uint8_t*>(&indices_buffer.data[acc.byteOffset + view.byteOffset]);
                    for (size_t j = 0; j < acc.count; ++j)
                    {
                        index_buffer.push_back(buffer[j] + vertex_start);
                        this_mesh.index_buffer.push_back(buffer[j]);
                    }
                    break;
                }
                default:
                    cout << "type of indices not supported." << "\n";
                    break;
            }

            // finally push the loaded primitive into the mesh's primitive vector.
            // atm any 'extra property' in the mesh is interpreted as being non-collideable -- prob expand this later (trigger colliders?).
            // ok so i can make a set of terms for extra properties

            // npc: name of the gltf model of the npc.
            // trigger: name of the gltf model of the target level? maybe expand it to the function to execute on trigger...
            
            // default to no extras meaning the mesh is solid.
            if (mesh.extras.IsObject())
            {
                


                // deal with extras.
                // mesh.extras.Get("name_of_property").Get<type_of_property>()
                cout << "NPC model: "       << mesh.extras.Get("npc").Get<std::string>() << "\n";
                cout << "Target level: "    << mesh.extras.Get("level").Get<int>() << "\n";

                this_mesh.type          = MeshPrimitive::Type::TRIGGER;
                this_mesh.target_level  = mesh.extras.Get("level").Get<int>();
                // this_mesh.spawn.x       = mesh.extras.Get<
            }
            else
            {
                this_mesh.type = MeshPrimitive::Type::COLLIDER;
            }

            this_mesh.first_index      = first_index;
            this_mesh.index_count      = static_cast<uint32_t>(acc.count);
            this_mesh.material_index   = gltf_primitive.material; // this is the id of the texture.
            node->mesh_primitives.push_back(this_mesh);
        }
    }

    if (parent)
    {
        parent->children.push_back(node);
    }
    else
    {
        nodes.push_back(node);
    }
}

// link vertex attributes such as position, normals, and texcoords to VBO.
void link_attrib(GLuint VBO, GLuint layout, GLuint size, GLenum type, GLsizeiptr stride, void *offset)
{
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glEnableVertexAttribArray(layout);

    switch (type)
    {
    case GL_FLOAT:
        glVertexAttribPointer(layout, size, type, GL_FALSE, stride, offset);
        break;
    case GL_INT:
        glVertexAttribIPointer(layout, size, type, stride, offset);
        break;
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// binds buffers when loading a mesh.
void bind_buffers(GLuint &VAO, GLuint &VBO, GLuint &EBO, std::vector<Vertex> vertices, std::vector<GLuint> indices)
{
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

    // link vertex attributes to shader layouts.
    link_attrib(VBO, 0, 3, GL_FLOAT, sizeof(Vertex), nullptr);                         // position (vec3).
    link_attrib(VBO, 1, 3, GL_FLOAT, sizeof(Vertex), (void *)(3   * sizeof(float)));   // normal   (vec3).
    link_attrib(VBO, 2, 3, GL_FLOAT, sizeof(Vertex), (void *)(6   * sizeof(float)));   // colour   (vec3).
    link_attrib(VBO, 3, 2, GL_FLOAT, sizeof(Vertex), (void *)(9   * sizeof(float)));   // texture  (vec2).
    link_attrib(VBO, 4, 4, GL_FLOAT, sizeof(Vertex), (void *)(11  * sizeof(float)));   // joints   (vec4).
    link_attrib(VBO, 5, 4, GL_FLOAT, sizeof(Vertex), (void *)(15  * sizeof(float)));   // weights  (vec4).

    glBindVertexArray(0);                       // unbind VAO.
    glBindBuffer(GL_ARRAY_BUFFER, 0);           // unbind VBO.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);   // unbind EBO.
}

void Model::bind_node(Node *node)
{
    if (node->mesh_primitives.size() > 0)
    {
        for (MeshPrimitive &mesh : node->mesh_primitives)
        {
            bind_buffers(mesh.VAO, mesh.VBO, mesh.EBO, mesh.vertex_buffer, mesh.index_buffer);
        }
    }

    for (auto &child : node->children)
    {
        bind_node(&*child);
    }
}

void Model::load_material(tinygltf::Model &input)
{
    for (size_t i = 0; i < input.textures.size(); ++i)
	{
        Material material;
        GLenum format;
        GLenum type;
        tinygltf::Image texture = input.images[input.textures[i].source];
        std::cout << "texture: " << texture.name << "\n";

        // generate texture using ID.
        glGenTextures(1, &material.texture_ID);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, material.texture_ID);

        // texture settings.
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);  // dunno what this does lol.
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // GL_LINEAR = bilinear filter.
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR); // GL_LINEAR_MIPMAP_LINEAR = trilinear filter.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        // determine image format from number of components. defaults to rgba.
        switch (texture.component)
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

        // determine image type from number of bits. defaults to 8 bit.
        switch (texture.bits)
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture.width, texture.height, 0, format, type, texture.image.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        // push material with id to vector in model.
        materials.push_back(material);
    }
}

// recrusive function to locate specific gltf node.
Node *Model::find_node(Node *parent, uint32_t index)
{
    Node *node_found = nullptr;
    if (parent->index == index)
    {
        return parent;
    }
    for (auto &child : parent->children)
    {
        node_found = find_node(child, index);
        if (node_found)
        {
            break;
        }
    }
    return node_found;
}

Node *Model::node_from_index(uint32_t index)
{
    Node *node_found = nullptr;
    for (auto &node : nodes)
    {
        node_found = find_node(node, index);
        if (node_found)
        {
            break;
        }
    }
    return node_found;
}

// basically just gets the inverse bind matrix of each node that is a joint.
void Model::load_skins(tinygltf::Model &input)
{
    // resize model's skin vector according to gltf being loaded.
    if (input.skins.size() > 0)
    {
        skins.resize(input.skins.size());

        // loop through each skin in the gltf model.
        for (size_t i = 0; i < input.skins.size(); ++i)
        {
            tinygltf::Skin gltf_skin    = input.skins[i];
            skins[i].skeleton_root      = node_from_index(gltf_skin.skeleton);
            skins[i].name               = gltf_skin.name;
            cout << "skin: " << skins[i].name << " (" << gltf_skin.joints.size() << " joints)\n";

            // find nodes that are joints.
            // "The order of joints is defined in the skin.joints array and it must match the order of inverseBindMatrices data."
            for (int joint_id : gltf_skin.joints)
            { 
                Node *node = node_from_index(joint_id);
                if (node)
                {
                    // cout << "Joint ID: " << node->index << "\n";
                    skins[i].joints.push_back(node);
                }
            }

            // get inverse bind matrices from buffer and push back to the skin.
            if (gltf_skin.inverseBindMatrices > -1)
            {
                const tinygltf::Accessor &acc       = input.accessors[gltf_skin.inverseBindMatrices];
                const tinygltf::BufferView &view    = input.bufferViews[acc.bufferView];
                const tinygltf::Buffer &buffer      = input.buffers[view.buffer];

                // copy buffer data to the skin's matrix vector.
                // acc.count is always equal (or greater than) the number of joints.
                skins[i].inverse_bind_matrices.resize(acc.count);
                memcpy(skins[i].inverse_bind_matrices.data(), &buffer.data[acc.byteOffset + view.byteOffset], acc.count * sizeof(glm::mat4));
            }

            if (skins[i].joints.size() > MAX_JOINTS)
            {
                std::cerr << "[WARNING] Skin " << skins[i].name << " has " << skins[i].joints.size() << " joints, which is higher than the supported maximum of " << MAX_JOINTS << "\n";
                std::cerr << "[WARNING] glTF scene may display wrong/incomplete\n";
            }
        }
    }
    else
    {
        // gltf is not skinned, so set skin vector to have 1 member.
        skins.resize(1);
        cout << "No skin, loaded empty joints.\n";
    }
}

// reset animation to initial frame.
void Animation::reset()
{
    current_time = start;
}

void Model::load_animations(tinygltf::Model &input)
{
    animations.resize(input.animations.size());

    for (size_t i = 0; i < input.animations.size(); ++i)
    {
        tinygltf::Animation gltf_animation  = input.animations[i];
        animations[i].name                  = gltf_animation.name;

        cout << "animation: " << animations[i].name <<  "\n";

        animations[i].samplers.resize(gltf_animation.samplers.size());
        for (size_t j = 0; j < gltf_animation.samplers.size(); ++j)
        {
            tinygltf::AnimationSampler gltf_sampler = gltf_animation.samplers[j];
            AnimationSampler &dst_sampler           = animations[i].samplers[j];
            dst_sampler.interpolation               = gltf_sampler.interpolation;

            // read sampler keyframe input time values.
            const tinygltf::Accessor &input_acc     = input.accessors[gltf_sampler.input];
            const tinygltf::BufferView &input_view  = input.bufferViews[input_acc.bufferView];
            const tinygltf::Buffer &input_buffer    = input.buffers[input_view.buffer];
            const void *input_data_ptr              = &input_buffer.data[input_acc.byteOffset + input_view.byteOffset];
            const float *input_buf                  = static_cast<const float *>(input_data_ptr);

            for (size_t index = 0; index < input_acc.count; ++index)
            {
                dst_sampler.inputs.push_back(input_buf[index]);
            }

            // adjust start & end times.
            for (auto input : animations[i].samplers[j].inputs)
            {
                if (input < animations[i].start)
                {
                    animations[i].start = input;
                }
                if (input > animations[i].end)
                {
                    animations[i].end = input;
                }
            }

            // get translate/rotate/scale from keyframe.
            const tinygltf::Accessor &output_acc    = input.accessors[gltf_sampler.output];
            const tinygltf::BufferView &output_view = input.bufferViews[output_acc.bufferView];
            const tinygltf::Buffer &output_buffer   = input.buffers[output_view.buffer];
            const void *output_data_ptr             = &output_buffer.data[output_acc.byteOffset + output_view.byteOffset];

            assert(output_acc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

            switch (output_acc.type)
            {
                case TINYGLTF_TYPE_VEC3: 
                {
                    const glm::vec3 *buffer = static_cast<const glm::vec3 *>(output_data_ptr);
                    for (size_t index = 0; index < output_acc.count; ++index)
                    {
                        dst_sampler.outputs_vec4.push_back(glm::vec4(buffer[index], 0.0f));
                    }
                    break;
                }
                case TINYGLTF_TYPE_VEC4: 
                {
                    const glm::vec4 *buffer = static_cast<const glm::vec4 *>(output_data_ptr);
                    for (size_t index = 0; index < output_acc.count; ++index)
                    {
                        dst_sampler.outputs_vec4.push_back(buffer[index]);
                    }
                    break;
                }
                default: 
                {
                    cout << "unknown type in animation: " << output_acc.type << "\n";
                    break;
                }
            }
        }

        // channels.
        animations[i].channels.resize(gltf_animation.channels.size());
        for (size_t j = 0; j < gltf_animation.channels.size(); ++j)
        {
            tinygltf::AnimationChannel gltf_channel = gltf_animation.channels[j];
            AnimationChannel &dst_channel           = animations[i].channels[j];
            dst_channel.path                        = gltf_channel.target_path;
            dst_channel.sampler_index               = gltf_channel.sampler;
            dst_channel.node                        = node_from_index(gltf_channel.target_node);
        }

        update_animations(0.0f, i);
    }
}

void Model::update_animations(float delta_time, uint32_t animation_index)
{
    if (animations.empty())
    {
        return;
    }

    if (animation_index > static_cast<uint32_t>(animations.size()) - 1)
    {
        // cout << "no animation with index: " << animation_index << "\n";
        return;
    }

    if (animation_index != active_animation)
    {
        // cout << "Animation: " << animation_index << "\n";
    }

    Animation &animation        = animations[animation_index];
    animation.current_time      += delta_time;

    // either loop animation, or let it play once.
    if (animation.current_time > animation.end)
    {
        if (animation.loop)
        {
            animation.current_time -= animation.end;
        }
        else
        {
            animation.current_time = animation.end;
        }
    }

    for (auto &channel : animation.channels)
    {
        AnimationSampler &sampler = animation.samplers[channel.sampler_index];
        for (size_t i = 0; i < sampler.inputs.size() - 1; ++i)
        {
            if (sampler.interpolation != "LINEAR")
            {
                cout << "only linear interpolations supported." << "\n";
                continue;
            }

            // get input keyframes for the current time value.
            if ((animation.current_time >= sampler.inputs[i]) && (animation.current_time <= sampler.inputs[i + 1]))
            {
                float interpolation     = (animation.current_time - sampler.inputs[i]) / (sampler.inputs[i + 1] - sampler.inputs[i]);
                glm::vec4 current_frame = sampler.outputs_vec4[i];
                glm::vec4 target_frame  = sampler.outputs_vec4[i + 1];

                if (channel.path == "translation")
                {
                    channel.node->translation = glm::mix(current_frame, target_frame, interpolation);
                }
            
                if (channel.path == "rotation")
                {
                    glm::quat current_quat  = glm::quat(current_frame.w,    current_frame.x,    current_frame.y,    current_frame.z);
                    glm::quat target_quat   = glm::quat(target_frame.w,     target_frame.x,     target_frame.y,     target_frame.z);
                    channel.node->rotation  = glm::normalize(glm::slerp(current_quat, target_quat, interpolation));
                }

                if (channel.path == "scale")
                {
                    channel.node->scale = glm::mix(current_frame, target_frame, interpolation);
                }
            }
        }
    }

    active_animation = animation_index;

    // finally, update the joints with the animation.
    for (auto &node : nodes)
    {
        update_joints(node);
    }
}

void Model::update_joints(Node *node)
{
    // if current node has a skin associated with it...:
    if (!node->mesh_primitives.empty())
    {
        auto m = get_node_matrix(node);
        if (node->skin > -1)
        {
            // get the inverse of the node containing the skin, as it needs to be ignored.
            glm::mat4 inverse_transform = glm::inverse(m);

            // loop through each joint in the skin.
            for (size_t i = 0; i < skins[node->skin].joints.size(); ++i)
            {
                // "jointMatrix[j] = inverse(globalTransform) * globalJointTransform[j] * inverseBindMatrix[j];"
                skins[node->skin].joint_matrix[i] = inverse_transform * get_node_matrix(skins[node->skin].joints[i]) * skins[node->skin].inverse_bind_matrices[i];
            }
        }
        else
        {
            // cout << "lol\n";
        }
    }
    
    for (auto &child : node->children)
    {
        update_joints(child);
    }
}

// load a model from a .gltf file (works with both combined and seperate, but not .glb).
Model::Model(std::string filename)
{
    tinygltf::Model glTF_input;         // stores .gltf model reference.
    tinygltf::TinyGLTF glTF_context;    // stores ASCII from file.
    std::string error;                  // outputs warning if fails to load properly.
    std::string warning;                // outputs error if any errors.

    bool loaded = glTF_context.LoadASCIIFromFile(&glTF_input, &error, &warning, MODELS_PATH + filename);
    if (!error.empty())     { cout << "ERR: " << error << "\n"; }
    if (!warning.empty())   { cout << "WARN: " << warning << "\n"; }

    std::vector<uint32_t> index_buffer;
    std::vector<Vertex> vertex_buffer;

    // if loaded correctly, load file contents.
    if (loaded)
    {
        cout << "model: " << filename << "\n";

        // load images, materials, textures.
        load_material(glTF_input);

        //glTF_input.defaultScene = glTF_input.scenes[0] (pretty sure).
        
        const tinygltf::Scene &scene = glTF_input.scenes[0];
        for (size_t i = 0; i < scene.nodes.size(); ++i)
        {
            const tinygltf::Node node = glTF_input.nodes[scene.nodes[i]];
            load_node(node, glTF_input, nullptr, scene.nodes[i], index_buffer, vertex_buffer);
        }

        // load skins and animations.
        load_skins(glTF_input);
        load_animations(glTF_input);

        // calculate initial pose.
        // verify this is working correctly!
        for (auto node : nodes)
		{
			update_joints(node);
		}

        // after loading everything, bind the mesh nodes.
        for (auto &node : nodes)
        {
            bind_node(&*node);
        }
    }
    cout << "\n";
}








// probably clean this stuff below up at some point,
// maybe keep the gltf loading/drawing to it's own file?

// // grid
// Grid::Grid(int slices)
// {
//     // set grid object slices to input.
//     Grid::slices = slices;

//     // generate vertices from slice number.
//     for(int i = 0; i <= slices; ++i)
//     {
//         for(int j = 0; j <= slices; ++j)
//         {
//             Vertex vertex;
//             vertex.position = glm::vec3((float)j/(float)slices, 0.0f, (float)i/(float)slices);
//             vertex.normal   = vertex.position;

//             // draws the x and z axis in seperate colours.
//             if ((i == slices/2) || (j == slices/2))
//             {
//                 vertex.color = glm::vec3(0.0f);
//             }
//             else
//             {
//                 vertex.color = glm::vec3(0.7f);
//             }
            
//             vertex.texUV    = glm::vec2(0.0f, 0.0f);
//             vertices.push_back(vertex);
//         }
//     }

//     // generate indices (in groups of 4).
//     for(int i = 0; i < slices; ++i)
//     {
//         for(int j = 0; j < slices; ++j)
//         {
//             int row1 = (slices + 1) * (i);
//             int row2 = (slices + 1) * (i +1);

//             indices.push_back(row1 + j);
//             indices.push_back(row1 + j + 1);
//             indices.push_back(row1 + j + 1);
//             indices.push_back(row2 + j + 1);
//             indices.push_back(row2 + j + 1);
//             indices.push_back(row2 + j);
//             indices.push_back(row2 + j);
//             indices.push_back(row1 + j);
//         }
//     }

//     // update grid mesh with new vertices and indices.
//     mesh = Mesh(vertices, indices, 0);
// }

// // draws grid at centre of world.
// void Grid::draw(Shader shader)
// {
//     int sca             = 1;
//     float xz_coord      = (float)((slices * -sca) / 2);
//     glm::vec3 position  = glm::vec3(xz_coord, 0.0f, xz_coord);
//     glm::quat rotation  = glm::quat(glm::vec3(0.0f));
//     glm::vec3 scale     = glm::vec3((float)(slices * sca));

//     // draw mesh with GL_LINES.
//     // mesh.draw(GL_LINES, position, rotation, scale, shader, glm::vec3(0.0f));
// }

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
    mesh.vertex_buffer.push_back(vertex);
    vertex.position = corners[1];
    mesh.vertex_buffer.push_back(vertex);
    vertex.position = corners[2];
    mesh.vertex_buffer.push_back(vertex);
    vertex.position = corners[3];
    mesh.vertex_buffer.push_back(vertex);

    // far face?
    vertex.position = corners[4];
    mesh.vertex_buffer.push_back(vertex);
    vertex.position = corners[5];
    mesh.vertex_buffer.push_back(vertex);
    vertex.position = corners[6];
    mesh.vertex_buffer.push_back(vertex);
    vertex.position = corners[7];
    mesh.vertex_buffer.push_back(vertex);


    mesh.index_buffer = {
        0, 1, 3, 3, 1, 2, // face
        1, 5, 2, 2, 5, 6,
        5, 4, 6, 6, 4, 7,
        4, 0, 7, 7, 0, 3,

        3, 2, 7, 7, 2, 6,

        3, 7, 3, 3, 7, 6
    };

    // fill joint matrix with placeholder values.
    for (size_t i = 0; i < MAX_JOINTS; ++i)
    {
        joint_matrix[i] = glm::mat4(1.0f);
    }

    bind_buffers(mesh.VAO, mesh.VBO, mesh.EBO, mesh.vertex_buffer, mesh.index_buffer);
}

// draws grid at centre of world.
void Frustum::draw(glm::vec3 position, Shader shader)
{
    glm::quat rotation  = glm::quat(glm::vec3(0.0f));
    glm::vec3 scale     = glm::vec3(1.0f);
    glm::mat4 mvp       = translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);

    // draw mesh with GL_LINES.
    glUseProgram(shader.ID);    // activate the shader being used to draw this model. (potential for multiple shaders per model?)
    
    // per mesh shader updates; colour and combined matrix transformation.
    // glUniform3fv(glGetUniformLocation(shader.ID, "albedo"), 1, glm::value_ptr(colour));
    // glUniformMatrix4fv(glGetUniformLocation(shader.ID, "view"), 1, GL_FALSE, glm::value_ptr(camera.mvp));
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "joint_matrices"), MAX_JOINTS, GL_FALSE, glm::value_ptr(joint_matrix[0]));
    
    glBindVertexArray(mesh.VAO);     // bind the VBO with the vertexes from the mesh.
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "mvp"), 1, GL_FALSE, glm::value_ptr(mvp));

    // set polygon mode and then draw elements.
    glPolygonMode(GL_FRONT_AND_BACK, shader.mode);
    glLineWidth(1.0f);
    glDrawElements(GL_LINES, mesh.index_buffer.size() * sizeof(mesh.index_buffer[0]), GL_UNSIGNED_INT, 0);

    // unbind vertex array and texture.
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
}

Line::Line(glm::vec3 angle, float length)
{
    mesh = MeshPrimitive();
    Vertex vertex_a, vertex_b;

    vertex_a.position         = glm::vec3(0.0f);
    vertex_a.normal           = vertex_a.position;
    vertex_a.color            = glm::vec3(0.0f);
    vertex_a.texUV            = glm::vec2(0.0f, 0.0f);
    vertex_a.joint_indices    = glm::vec4(1.0f);
    vertex_a.joint_weights    = glm::vec4(1.0f);

    mesh.vertex_buffer.push_back(vertex_a);
    mesh.index_buffer.push_back(0);

    vertex_b.position         = vertex_a.position + (angle * length);
    vertex_b.normal           = vertex_b.position;
    vertex_b.color            = glm::vec3(0.0f);
    vertex_b.texUV            = glm::vec2(0.0f, 0.0f);
    vertex_b.joint_indices    = glm::vec4(1.0f);
    vertex_b.joint_weights    = glm::vec4(1.0f);

    mesh.vertex_buffer.push_back(vertex_b);
    mesh.index_buffer.push_back(1);

    // fill joint matrix with placeholder values.
    for (size_t i = 0; i < MAX_JOINTS; ++i)
    {
        joint_matrix[i] = glm::mat4(1.0f);
    }

    bind_buffers(mesh.VAO, mesh.VBO, mesh.EBO, mesh.vertex_buffer, mesh.index_buffer);
}

void Line::draw(glm::vec3 position, glm::quat rotation, Shader shader, glm::vec3 colour)
{
    // glm::quat rotation  = glm::quat(glm::vec3(0.0f));
    glm::vec3 scale     = glm::vec3(1.0f);
    glm::mat4 mvp       = translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);

    // draw mesh with GL_LINES.
    glUseProgram(shader.ID);    // activate the shader being used to draw this model. (potential for multiple shaders per model?)
    
    // per mesh shader updates; colour and combined matrix transformation.
    glUniform3fv(glGetUniformLocation(shader.ID, "albedo"), 1, glm::value_ptr(colour));   
    // glUniformMatrix4fv(glGetUniformLocation(shader.ID, "view"), 1, GL_FALSE, glm::value_ptr(camera.mvp));
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "joint_matrices"), MAX_JOINTS, GL_FALSE, glm::value_ptr(joint_matrix[0]));
    
    glBindVertexArray(mesh.VAO);     // bind the VBO with the vertexes from the mesh.
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "mvp"), 1, GL_FALSE, glm::value_ptr(mvp));

    // set polygon mode and then draw elements.
    glPolygonMode(GL_FRONT_AND_BACK, shader.mode);
    glLineWidth(1.0f);
    glDrawElements(GL_LINE_LOOP, mesh.index_buffer.size() * sizeof(mesh.index_buffer[0]), GL_UNSIGNED_INT, 0);

    // unbind vertex array and texture.
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
}

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
void Circle::draw(glm::vec3 position, Shader shader, Camera camera, glm::vec3 colour)
{
    glm::quat rotation  = glm::quat(glm::vec3(0.0f));
    glm::vec3 scale     = glm::vec3(1.0f);
    glm::mat4 mvp       = translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);

    // draw mesh with GL_LINES.
    glUseProgram(shader.ID);    // activate the shader being used to draw this model. (potential for multiple shaders per model?)
    
    // per mesh shader updates; colour and combined matrix transformation.
    glUniform3fv(glGetUniformLocation(shader.ID, "albedo"), 1, glm::value_ptr(colour));   
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "view"), 1, GL_FALSE, glm::value_ptr(camera.mvp));
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "joint_matrices"), MAX_JOINTS, GL_FALSE, glm::value_ptr(joint_matrix[0]));
    
    glBindVertexArray(mesh.VAO);     // bind the VBO with the vertexes from the mesh.
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "mvp"), 1, GL_FALSE, glm::value_ptr(mvp));

    // set polygon mode and then draw elements.
    glPolygonMode(GL_FRONT_AND_BACK, shader.mode);
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
void ScreenTexture::draw(Shader &screen_shader, Shader &blur_shader)
{
    bool horizontal = true;
    if (bloom)
    {
        
        bool first_iteration    = true;
        unsigned int amount     = 10;

        glUseProgram(blur_shader.ID);
        glBindVertexArray(VAO);

        for (unsigned int i = 0; i < amount; ++i)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpong_FBO[horizontal]);
            glUniform1i(glGetUniformLocation(blur_shader.ID, "horizontal"), horizontal);
            glBindTexture(GL_TEXTURE_2D, first_iteration ? color_buffers[1] : pingpong_buffers[!horizontal]);  // bind texture of other framebuffer (or scene if first iteration)

            // draw pingpong buffer.
            glPolygonMode(GL_FRONT_AND_BACK, screen_shader.mode);
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
    glUseProgram(screen_shader.ID);
    glBindVertexArray(VAO);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, color_buffers[0]);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, pingpong_buffers[!horizontal]);

    // send gamma to post process shader.
    glUniform1i(glGetUniformLocation(screen_shader.ID, "screen_texture"), 0);
    glUniform1i(glGetUniformLocation(screen_shader.ID, "bloom"), 1);
    glUniform1f(glGetUniformLocation(screen_shader.ID, "gamma"), gamma);

    // draw the framebuffer.
    glPolygonMode(GL_FRONT_AND_BACK, screen_shader.mode);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glEnable(GL_DEPTH_TEST);


    // unbind vertex array and texture.
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
}


Skybox::Skybox(int level_index)
{
//     float vertices[] = {
//     // positions          
//     -1.0f,  1.0f, -1.0f,
//     -1.0f, -1.0f, -1.0f,
//      1.0f, -1.0f, -1.0f,
//      1.0f, -1.0f, -1.0f,
//      1.0f,  1.0f, -1.0f,
//     -1.0f,  1.0f, -1.0f,

//     -1.0f, -1.0f,  1.0f,
//     -1.0f, -1.0f, -1.0f,
//     -1.0f,  1.0f, -1.0f,
//     -1.0f,  1.0f, -1.0f,
//     -1.0f,  1.0f,  1.0f,
//     -1.0f, -1.0f,  1.0f,

//      1.0f, -1.0f, -1.0f,
//      1.0f, -1.0f,  1.0f,
//      1.0f,  1.0f,  1.0f,
//      1.0f,  1.0f,  1.0f,
//      1.0f,  1.0f, -1.0f,
//      1.0f, -1.0f, -1.0f,

//     -1.0f, -1.0f,  1.0f,
//     -1.0f,  1.0f,  1.0f,
//      1.0f,  1.0f,  1.0f,
//      1.0f,  1.0f,  1.0f,
//      1.0f, -1.0f,  1.0f,
//     -1.0f, -1.0f,  1.0f,

//     -1.0f,  1.0f, -1.0f,
//      1.0f,  1.0f, -1.0f,
//      1.0f,  1.0f,  1.0f,
//      1.0f,  1.0f,  1.0f,
//     -1.0f,  1.0f,  1.0f,
//     -1.0f,  1.0f, -1.0f,

//     -1.0f, -1.0f, -1.0f,
//     -1.0f, -1.0f,  1.0f,
//      1.0f, -1.0f, -1.0f,
//      1.0f, -1.0f, -1.0f,
//     -1.0f, -1.0f,  1.0f,
//      1.0f, -1.0f,  1.0f
// };

//     // generate VAO.
//     glGenVertexArrays(1, &VAO);
//     glBindVertexArray(VAO);

//     // generate VBO, bind, then pass vertices vector.
//     glGenBuffers(1, &VBO);
//     glBindBuffer(GL_ARRAY_BUFFER, VBO);
//     glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);

//     // link attribs.
//     link_attrib(VBO, 0, 3, GL_FLOAT, 3 * sizeof(float), (void*)(0)); // positions (vec3).

    std::string current_skybox = TEXTURES_PATH + std::string("skybox_" + std::to_string(level_index) + "/");

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
            cout << "Failed to load cubemap texture: " << filenames[i] << "\n";
            stbi_image_free(data);
        }

        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
        
        cout << "Loaded cubemap texture: " << filenames[i] << "\n";
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void Skybox::draw(glm::vec3 position, Shader shader, Camera camera)
{
    glm::mat4 transform = translate(glm::mat4(1.0f), camera.get_position(position))* glm::mat4_cast(glm::quat(glm::vec3(0.0f)))* glm::scale(glm::mat4(1.0f), glm::vec3(camera.FAR_PLANE));

    glUseProgram(shader.ID);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, ID);
    glCullFace(GL_FRONT);
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "view"), 1, GL_FALSE, glm::value_ptr(camera.mvp));

    // loop through all nodes in the model.
    for (auto &node : cube_mesh.nodes)
    {
        cube_mesh.draw_node(*node, GL_TRIANGLES, transform, shader);
    }
    
    // would be nice to get this to work so don't need to literally load a gltf cube.
    // or, could make a 'model' from the vertices?

    // glDepthFunc(GL_LEQUAL);
    // glUniformMatrix4fv(glGetUniformLocation(shader.ID, "mvp"), 1, GL_FALSE, glm::value_ptr(transform));
    // glBindVertexArray(VAO);
    // glActiveTexture(GL_TEXTURE0);
    // glBindTexture(GL_TEXTURE_CUBE_MAP, ID);
    // glDrawArrays(GL_TRIANGLES, 0, 36);
    // glBindVertexArray(0);


    // glDepthFunc(GL_LESS);
    glCullFace(GL_BACK);
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

    // unbind VAO and texture.
    glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArray(0);
}