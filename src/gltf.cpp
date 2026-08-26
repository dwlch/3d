#include "gltf.hpp"

#include "defines.hpp"                  // window dimensions.
#include <filesystem>                   // file extension parsing
#include <iostream>                     // std::cout etc.
#include <typeinfo>
#include <glm/gtc/type_ptr.hpp>         // get type of pointer for shaders.
#include <stb_image.h>                  // load images (include seperately from tinygltf).
#include <type_traits>


#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION        // stb image for textures.
#define STB_IMAGE_WRITE_IMPLEMENTATION  // stb image write for tinygltf.
#include <tiny_gltf.h>                  // actually include the file.



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

void glTF::draw_node(Node node, GLenum mode, glm::mat4 transform, int shader, Frustum frustum, int &render_count)
{
    // if AABB of the node is within the camera frustum OR it's a shadow/skybox/(and rn, player/npc): draw. otherwise skip.
    if (frustum.is_inside(node.min, node.max) || shader == 6 || shader == 18 || shader == 12)
    {
        // draw mesh of node.
        if (node.mesh_primitives.size())
        {
            // node matrix combined with model transform.
            glm::mat4 node_transform = transform * get_node_matrix(&node);
            
            // loop through each mesh in the node (usually just one atm).
            for (MeshPrimitive &mesh : node.mesh_primitives)
            {
                if (mesh.index_count > 0)
                {
                    // bind the VAO with the vertexes from the mesh.
                    glBindVertexArray(mesh.VAO);    
                    glUniformMatrix4fv(glGetUniformLocation(shader, "mvp"), 1, GL_FALSE, glm::value_ptr(node_transform));

                    // if mesh has a material/texture attached to it.
                    if (mesh.material_index > -1)
                    {
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, materials[mesh.material_index].texture_ID);
                        glUniform1i(glGetUniformLocation(shader, "tex0"), 0);
                    }

                    // maybe rather than shader.mode, set when loading trigger vs mesh?
                    // set polygon mode and then draw elements.
                    // glPolygonMode(GL_FRONT_AND_BACK, shader.mode);
                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                    glDrawElements(mode, mesh.index_buffer.size() * sizeof(mesh.index_buffer[0]), GL_UNSIGNED_INT, 0);

                    // unbind vertex array and texture.
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glBindVertexArray(0);

                    ++render_count;
                }
            }
        }
    }
    
    for (auto &child : node.children)
    {
        draw_node(*child, mode, transform, shader, frustum, render_count);
    }
}

// draw model by drawing each mesh contained within the model.
void glTF::draw(glm::vec3 position, glm::quat rotation, glm::vec3 scale, int shader, Camera camera, glm::vec3 colour)
{
    int render_count    = 0;
    Frustum frustum     = Frustum(camera.mvp);
    glm::mat4 transform = translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);

    // per-model uniforms.
    glUseProgram(shader);
    glUniform1f(glGetUniformLocation(shader, "camera_distance"), camera.distance_offset);
    glUniform1fv(glGetUniformLocation(shader, "cascade_bounds"), NUM_CASCADES, reinterpret_cast<GLfloat*>(camera.cascade_bounds.data()));
    glUniform3fv(glGetUniformLocation(shader, "albedo"), 1, glm::value_ptr(colour));
    glUniform3fv(glGetUniformLocation(shader, "light_pos"), 1, glm::value_ptr(camera.light_pos));
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, glm::value_ptr(camera.mvp));
    glUniformMatrix4fv(glGetUniformLocation(shader, "light"), NUM_CASCADES, GL_FALSE, reinterpret_cast<GLfloat*>(camera.cascade_proj.data()));
    glLineWidth(1.0f);
    
    for (int i = 0; i < NUM_CASCADES; ++i)
    { 
        // bind from texture1 onwards. texture0 is for mesh textures atm.
        glActiveTexture(GL_TEXTURE1 + i);
        glBindTexture(GL_TEXTURE_2D, camera.depth_maps[i]);
        glUniform1i(glGetUniformLocation(shader, std::string("shadow_map[" + std::to_string(i) + "]").c_str()), i + 1);
    }

    for (auto skin : skins)
    {
        // glUniformMatrix4fv(glGetUniformLocation(shader.ID, "joint_matrices"), MAX_JOINTS, GL_FALSE, glm::value_ptr(skin.joint_matrix[0]));
        glUniformMatrix4fv(glGetUniformLocation(shader, "joint_matrices"), MAX_JOINTS, GL_FALSE, reinterpret_cast<GLfloat*>(skin.joint_matrix.data()));
    }

    // loop through all nodes in the model.
    for (auto &node : nodes)
    {
        draw_node(*node, GL_TRIANGLES, transform, shader, frustum, render_count);
    }

    if (shader != 6 && shader != 18 && shader != 12)
    {
        // std::cout << render_count << "\n";
    }
    
}




template <typename T> T glTF::get_node_extras(const tinygltf::Value *extras, std::string property_name)
{
    T property = nullptr;
    if (extras->IsObject())
    {
        if (extras->Has(property_name))
        {
            if (std::is_same<T, int>::value)
            {
                std::cout << property_name << ": " << extras->Get(property_name).GetNumberAsInt() << "\n";
                property = extras->Get(property_name).GetNumberAsInt();
            }
            else if ((std::is_same<T, double>::value))
            {
                std::cout << property_name << ": " << extras->Get(property_name).GetNumberAsDouble() << "\n";
                property = extras->Get(property_name).GetNumberAsDouble();
            }
            else if ((std::is_same<T, glm::vec3>::value))
            {
                property = glm::vec3(   (float)extras->Get(property_name).Get(0).GetNumberAsDouble(),
                                        (float)extras->Get(property_name).Get(1).GetNumberAsDouble(),
                                        (float)extras->Get(property_name).Get(2).GetNumberAsDouble());
            }
        }
    }
    return property;
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

void glTF::load_node(const tinygltf::Node &input_node, tinygltf::Model &input, Node *parent, uint32_t node_index, std::vector<uint32_t> &index_buffer, std::vector<Vertex> &vertex_buffer)
{
    Node *node      = new Node{};
    node->parent    = parent;
    node->index     = node_index;
    node->skin      = input_node.skin;

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
        // std::cout << "Mesh: " << mesh.name << "\n";

        // loop through each primitive of the mesh (usually 1 per mesh):
        for (size_t i = 0; i < mesh.primitives.size(); ++i)
        {
            MeshPrimitive this_mesh{}; // where what is loaded is stored.
            const tinygltf::Primitive &gltf_primitive = mesh.primitives[i];
            uint32_t first_index    = static_cast<uint32_t>(index_buffer.size());
            uint32_t vertex_start   = static_cast<uint32_t>(vertex_buffer.size());
            
            // byte strides/lengths for each buffer entry.
            int position_stride     = 0;
            int normal_stride       = 0;
            int texcoord_stride     = 0;
            int colours_stride      = 0;
            int colours_stride2     = 0;
            int joints_stride       = 0;
            int weights_stride      = 0;

            // retrieve all buffers.
            const auto positions_buffer     = get_buffer<float> (input, gltf_primitive, "POSITION",     TINYGLTF_TYPE_VEC3, position_stride);
            const auto normals_buffer       = get_buffer<float> (input, gltf_primitive, "NORMAL",       TINYGLTF_TYPE_VEC3, normal_stride);
            const auto texcoords_buffer     = get_buffer<float> (input, gltf_primitive, "TEXCOORD_0",   TINYGLTF_TYPE_VEC2, texcoord_stride);
            const auto colours_buffer       = get_buffer<float> (input, gltf_primitive, "COLOR_0",      TINYGLTF_TYPE_VEC4, colours_stride);
            const auto colours_buffer2      = get_buffer<float> (input, gltf_primitive, "COLOR_1",      TINYGLTF_TYPE_VEC4, colours_stride2);
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
                vertex.normal   = normals_buffer    ? glm::normalize(glm::make_vec3(&normals_buffer[j * normal_stride]))    : glm::vec3(0.0f);
                vertex.texUV    = texcoords_buffer  ? glm::make_vec2(&texcoords_buffer[j * texcoord_stride])                : glm::vec2(0.0f);
                vertex.color    = colours_buffer    ? glm::vec3(glm::make_vec4(&colours_buffer[j * colours_stride]))        : glm::vec3(1.0f);
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
                            std::cout << "Joint component type not supported.\n"; 
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

                glm::vec3 collider_vertex = glm::vec3(get_node_matrix(node) * glm::vec4(vertex.position, 1.0f));

                // for collider, apply mvp to vertex position exactly like in shader.
                node->collision_vertices.push_back(collider_vertex);
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
                    std::cout << "type of indices not supported." << "\n";
                    break;
            }

            // finally push the loaded primitive into the mesh's primitive vector.
            this_mesh.first_index       = first_index;
            this_mesh.index_count       = static_cast<uint32_t>(acc.count);
            this_mesh.material_index    = gltf_primitive.material; // this is the id of the texture.
            
            node->mesh_primitives.push_back(this_mesh);


            // auto xExtremes  = std::minmax_element(node->collision_vertices.begin(), node->collision_vertices.end(), [](const glm::vec3& lhs, const glm::vec3& rhs) { return lhs.x < rhs.x; });
            // auto yExtremes  = std::minmax_element(node->collision_vertices.begin(), node->collision_vertices.end(), [](const glm::vec3& lhs, const glm::vec3& rhs) { return lhs.y < rhs.y; });
            // auto zExtremes  = std::minmax_element(node->collision_vertices.begin(), node->collision_vertices.end(), [](const glm::vec3& lhs, const glm::vec3& rhs) { return lhs.z < rhs.z; });

            // node->min       = glm::vec3(xExtremes.first->x,     yExtremes.first->y,     zExtremes.first->z);
            // node->max       = glm::vec3(xExtremes.second->x,    yExtremes.second->y,    zExtremes.second->z);

            glm::vec3 min_point(std::numeric_limits<float>::max());
            glm::vec3 max_point(std::numeric_limits<float>::lowest());

            for (const glm::vec3& vertex : node->collision_vertices)
            {
                min_point = glm::min(min_point, vertex);
                max_point = glm::max(max_point, vertex);
            }

            node->min = min_point;
            node->max = max_point;

        }
    }

    // get the custom properties from the gltf file.
    // this is still WIP; can probably make this a more generic function that just gets the custom properties of any given node.
    get_extras(&input_node, node);

    if (parent)
    {
        parent->children.push_back(node);
    }
    else
    {
        nodes.push_back(node);
    }
}

void glTF::bind_node(Node *node)
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

void glTF::load_material(tinygltf::Model &input)
{
    for (size_t i = 0; i < input.textures.size(); ++i)
	{
        Material material;
        GLenum format;
        GLenum type;
        tinygltf::Image texture = input.images[input.textures[i].source];
        // std::cout << "texture: " << texture.name << "\n";

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
Node *glTF::find_node(Node *parent, uint32_t index)
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

Node *glTF::node_from_index(uint32_t index)
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
void glTF::load_skins(tinygltf::Model &input)
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
            // std::cout << "skin: " << skins[i].name << " (" << gltf_skin.joints.size() << " joints)\n";

            // find nodes that are joints.
            // "The order of joints is defined in the skin.joints array and it must match the order of inverseBindMatrices data."
            for (int joint_id : gltf_skin.joints)
            { 
                Node *node = node_from_index(joint_id);
                if (node)
                {
                    // std::cout << "Joint ID: " << node->index << "\n";
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
        // std::cout << "No skin, loaded empty joints.\n";
    }
}

// reset animation to initial frame.
void Animation::reset()
{
    current_time = start;
}

void glTF::load_animations(tinygltf::Model &input)
{
    animations.resize(input.animations.size());

    for (size_t i = 0; i < input.animations.size(); ++i)
    {
        tinygltf::Animation gltf_animation  = input.animations[i];
        animations[i].name                  = gltf_animation.name;

        // std::cout << "animation: " << animations[i].name <<  "\n";

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
                    std::cout << "unknown type in animation: " << output_acc.type << "\n";
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

void glTF::update_animations(float delta_time, uint32_t animation_index)
{
    if (animations.empty())
    {
        return;
    }

    if (animation_index > static_cast<uint32_t>(animations.size()) - 1)
    {
        // std::cout << "no animation with index: " << animation_index << "\n";
        return;
    }

    // // if animation changes.
    // bool is_new_animation   = false;
    // int prev_animation      = animation_index;

    // if (animation_index != active_animation)
    // {
    //     is_new_animation    = true;
    //     prev_animation      = active_animation;
    //     std::cout << "Animation: " << animation_index << "\n";
    // }

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
                std::cout << "only linear interpolations supported." << "\n";
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

void glTF::update_joints(Node *node)
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
            // std::cout << "lol\n";
        }
    }
    
    for (auto &child : node->children)
    {
        update_joints(child);
    }
}

// load a model from a .gltf file (works with both combined and seperate, but not .glb).
void glTF::load_from_file(std::string filename)
{
    tinygltf::Model glTF_input;         // stores .gltf model reference.
    tinygltf::TinyGLTF glTF_context;    // stores ASCII from file.
    std::string error;                  // outputs warning if fails to load properly.
    std::string warning;                // outputs error if any errors.
    bool loaded = false;

    if (std::filesystem::path(filename).extension() == ".glb")
    {
        loaded = glTF_context.LoadBinaryFromFile(&glTF_input, &error, &warning, MODELS_PATH + filename);
    }
    else if (std::filesystem::path(filename).extension() == ".gltf")
    {
        loaded = glTF_context.LoadASCIIFromFile(&glTF_input, &error, &warning, MODELS_PATH + filename);
    }
    else
    {
        std::cout << "Invalid file extension (only accepts .glb and .gltf)\n";
    }
    
    if (!error.empty())
    {
        std::cout << "ERR: " << error << "\n";
    }
    if (!warning.empty())
    {
        std::cout << "WARN: " << warning << "\n";
    }

    // store the indices and vertices for all meshes continuously in vectors.
    // atm this is unused, theyre being stored per-mesh rn.
    std::vector<uint32_t> index_buffer;
    std::vector<Vertex> vertex_buffer;

    // if loaded correctly, load file contents.
    if (loaded)
    {
        std::cout << "model: " + filename << "\n";

        // load images, materials, textures.
        load_material(glTF_input);

        //glTF_input.scenes[0] = glTF_input.scenes[0] (pretty sure).
        assert(glTF_input.scenes[0] == glTF_input.scenes[0]);
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
}