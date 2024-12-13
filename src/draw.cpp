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
    if (node.mesh_primitives.size() > 0)
    {
        // node matrix combined with model transform.
        glm::mat4 mvp = transform * get_node_matrix(&node);

        // loop through each mesh in the node (usually just one atm).
        for (MeshPrimitive &mesh : node.mesh_primitives)
        {
            if (mesh.index_count > 0)
            {
                // bind the VAO with the vertexes from the mesh.
                glBindVertexArray(mesh.VAO);    
                glUniformMatrix4fv(glGetUniformLocation(shader.ID, "mvp"), 1, GL_FALSE, glm::value_ptr(mvp));   // mesh view projection matrix.

                if (mesh.material_index > -1)
                {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, textures[mesh.material_index].ID);
                    glUniform1i(glGetUniformLocation(shader.ID, "tex0"), 0);
                }
                // set polygon mode and then draw elements.
                glPolygonMode(GL_FRONT_AND_BACK, shader.mode);
                glLineWidth(1.0f);
                glDrawElements(mode, mesh.index_buffer.size() * sizeof(mesh.index_buffer[0]), GL_UNSIGNED_INT, 0);

                // unbind vertex array and texture.
                glBindTexture(GL_TEXTURE_2D, 0);
                glBindVertexArray(0);
            }
        }
    }

    for (auto &child : node.children)
    {
        draw_node(*child, mode, transform, shader);
    }

}

// draw model by drawing each mesh contained within the model.
void Model::draw(glm::vec3 position, glm::quat rotation, glm::vec3 scale, Shader shader, glm::vec3 colour)
{
    glm::mat4 transform = translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);

    // per-model uniforms.
    glUseProgram(shader.ID);
    glUniform3fv(glGetUniformLocation(shader.ID, "albedo"), 1, glm::value_ptr(colour));
    for (auto skin : skins)
    {
        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "joint_matrices"), MAX_JOINTS, GL_FALSE, glm::value_ptr(skin.joint_matrix[0]));
    }

    // loop through all nodes in the model.
    for (auto &node : nodes)
    {
        draw_node(*node, GL_TRIANGLES, transform, shader);
    }
}

// texture handling, more tinygltf agnostic now.
Texture::Texture(std::string &filename, int &width, int &height, int &component, int &bits, unsigned char *image_data)
{
    Texture::name = filename;
    std::cout << "texture: " << name << "\n";

    // generate texture using ID.
    glGenTextures(1, &ID);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ID);

    // texture settings.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);  // dunno what this does lol.
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // GL_LINEAR = bilinear filter.
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR); // GL_LINEAR_MIPMAP_LINEAR = trilinear filter.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // determine image format from number of components. defaults to rgba.
    switch (component)
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
    switch (bits)
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, format, type, image_data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // unbind texture.
    glBindTexture(GL_TEXTURE_2D, 0);
}

template <typename T> const T *get_buffer(tinygltf::Model &model, tinygltf::Primitive primitive, std::string name, int type, int &stride, size_t &count)
{
    const T *buffer = nullptr;
    if (primitive.attributes.find(name) != primitive.attributes.end())
    {
        const tinygltf::Accessor &acc       = model.accessors[primitive.attributes.find(name)->second];
        const tinygltf::BufferView &view    = model.bufferViews[acc.bufferView];
        buffer                              = reinterpret_cast<const T *>(&model.buffers[view.buffer].data[view.byteOffset + acc.byteOffset]);
        stride                              = acc.ByteStride(view) ? (acc.ByteStride(view) / sizeof(T)) : tinygltf::GetNumComponentsInType(type);
        count                               = acc.count;
    }
    return buffer;
}

void Model::load_node(const tinygltf::Node &input_node, tinygltf::Model &input, Node *parent, uint32_t node_index, std::vector<uint32_t> &index_buffer, std::vector<Vertex> &vertex_buffer)
{
    Node *node      = new Node{};
    node->parent    = parent;
    node->matrix    = glm::mat4(1.0f);
    node->index     = node_index;
    node->skin      = input_node.skin;

    // get the node's transform, either as in T*R*S format, or a matrix.
    if (!input_node.translation.empty())
    {
        node->translation = glm::make_vec3(input_node.translation.data());
    }
    if (!input_node.rotation.empty())
    {
        glm::quat q     = glm::make_quat(input_node.rotation.data());
        node->rotation  = glm::mat4(q);
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
        // loop through each primitive of the mesh, which is usually only 1 per mesh.
        for (size_t i = 0; i < mesh.primitives.size(); ++i)
        {
            const tinygltf::Primitive &gltf_primitive = mesh.primitives[i];
            uint32_t first_index    = static_cast<uint32_t>(index_buffer.size());
            uint32_t vertex_start   = static_cast<uint32_t>(vertex_buffer.size());
            bool has_skin           = false;
            
            // where what is loaded is stored.
            MeshPrimitive mesh_primitive{};
            std::vector<Vertex> collider_vertices;       // store vertices publicly for collision load?
            std::vector<GLuint> collider_indices;

            cout << "Mesh: " << mesh.name << "\n";

            // byte strides/lengths for each buffer entry.
            int position_stride     = 0;
            int normal_stride       = 0;
            int texcoord_stride     = 0;
            int joints_stride       = 0;
            int weights_stride      = 0;
            
            uint32_t index_count    = 0;
            size_t vertices_count   = 0;

            // these are unnecesary, find a way to get rid.
            size_t normals_count    = 0;
            size_t texcoords_count  = 0;
            size_t joints_count     = 0;
            size_t weights_count    = 0;

            // retrieve all buffers.
            const auto positions_buffer     = get_buffer<float>(    input, gltf_primitive, "POSITION",      TINYGLTF_TYPE_VEC3, position_stride,    vertices_count);
            const auto normals_buffer       = get_buffer<float>(    input, gltf_primitive, "NORMAL",        TINYGLTF_TYPE_VEC3, normal_stride,      normals_count);
            const auto texcoords_buffer     = get_buffer<float>(    input, gltf_primitive, "TEXCOORD_0",    TINYGLTF_TYPE_VEC2, texcoord_stride,    texcoords_count);
            const auto joint_indices_buffer = get_buffer<uint16_t>( input, gltf_primitive, "JOINTS_0",      TINYGLTF_TYPE_VEC4, joints_stride,      joints_count);
            const auto joint_weights_buffer = get_buffer<float>(    input, gltf_primitive, "WEIGHTS_0",     TINYGLTF_TYPE_VEC4, weights_stride,     weights_count);

            // if the node has joint indides & weights, then it is skinned.
            has_skin = (joint_indices_buffer && joint_weights_buffer);

            for (size_t j = 0; j < vertices_count; ++j)
            {
                Vertex vertex{};
                vertex.position         = glm::vec4(glm::make_vec3(&positions_buffer[j * position_stride]), 1.0f);
                vertex.normal           = glm::normalize(glm::vec3(normals_buffer ? glm::make_vec3(&normals_buffer[j * normal_stride]) : glm::vec3(0.0f)));
                vertex.texUV            = texcoords_buffer ? glm::make_vec2(&texcoords_buffer[j * texcoord_stride]) : glm::vec2(0.0f);
                vertex.color            = vertex.normal;
                vertex.joint_indices    = has_skin ? glm::vec4(glm::make_vec4(&joint_indices_buffer[j * joints_stride])) : glm::vec4(1.0f);
                vertex.joint_weights    = has_skin ? glm::vec4(glm::make_vec4(&joint_weights_buffer[j * weights_stride])) : glm::vec4(1.0f);

                // clean this up.
                vertex_buffer.push_back(vertex);
                mesh_primitive.vertex_buffer.push_back(vertex);
                
                vertex.position = glm::vec3(get_node_matrix(node) * glm::vec4(glm::make_vec3(&positions_buffer[j * position_stride]), 1.0f));
                collider_vertices.push_back(vertex);
                mesh_primitive.vertex_collider_buffer.push_back(vertex);
            }

            // indices.
            const tinygltf::Accessor &acc           = input.accessors[gltf_primitive.indices];
            const tinygltf::BufferView &view        = input.bufferViews[acc.bufferView];
            const tinygltf::Buffer &indices_buffer  = input.buffers[view.buffer];
            index_count += static_cast<uint32_t>(acc.count);

            switch (acc.componentType)
            {
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                {
                        const uint32_t *buffer = reinterpret_cast<const uint32_t*>(&indices_buffer.data[acc.byteOffset + view.byteOffset]);
						for (size_t j = 0; j < acc.count; ++j)
						{
							index_buffer.push_back(buffer[j] + vertex_start);
                            
                            mesh_primitive.index_buffer.push_back(buffer[j]);
                            collider_indices.push_back(buffer[j]);
						}
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: // 5123, mostly going here i think.
                {
                    const uint16_t *buffer = reinterpret_cast<const uint16_t*>(&indices_buffer.data[acc.byteOffset + view.byteOffset]);
                    for (size_t j = 0; j < acc.count; ++j)
                    {
                        index_buffer.push_back(buffer[j] + vertex_start);

                        mesh_primitive.index_buffer.push_back(buffer[j]);
                        collider_indices.push_back(buffer[j]);
                    }
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                {
                    const uint8_t *buffer = reinterpret_cast<const uint8_t*>(&indices_buffer.data[acc.byteOffset + view.byteOffset]);
                    for (size_t j = 0; j < acc.count; ++j)
                    {
                        index_buffer.push_back(buffer[j] + vertex_start);

                        mesh_primitive.index_buffer.push_back(buffer[j]);
                        collider_indices.push_back(buffer[j]);
                    }
                    break;
                }
                default:
                    cout << "type of indices not supported." << "\n";
                    break;
            }

            // finally push the loaded primitive into the mesh's primitive vector.
            mesh_primitive.first_index      = first_index;
            mesh_primitive.index_count      = index_count;
            mesh_primitive.material_index   = gltf_primitive.material; // this is the id of the texture.
            node->mesh_primitives.push_back(mesh_primitive);

            // testing.
            meshes.push_back(Mesh(collider_vertices, collider_indices, 0));
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

void Model::bind_node(Node *node)
{
    if (node->mesh_primitives.size() > 0)
    {
        for (MeshPrimitive &mesh : node->mesh_primitives)
        {
            glGenVertexArrays(1, &mesh.VAO); // generate VAO.
            glBindVertexArray(mesh.VAO);     // bind VAO.

            // generate VBO, bind, then pass vertices vector.
            glGenBuffers(1, &mesh.VBO);
            glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
            glBufferData(GL_ARRAY_BUFFER, mesh.vertex_buffer.size() * sizeof(Vertex), mesh.vertex_buffer.data(), GL_STATIC_DRAW);

            // generate EBO, bind, then pass indices vector.
            glGenBuffers(1, &mesh.EBO);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.index_buffer.size() * sizeof(GLuint), mesh.index_buffer.data(), GL_STATIC_DRAW);

            // link vertex attributes to shader layouts.
            link_attrib(mesh.VBO, 0, 3, GL_FLOAT, sizeof(Vertex), nullptr);                         // position (vec3).
            link_attrib(mesh.VBO, 1, 3, GL_FLOAT, sizeof(Vertex), (void *)(3   * sizeof(float)));   // normal   (vec3).
            link_attrib(mesh.VBO, 2, 3, GL_FLOAT, sizeof(Vertex), (void *)(6   * sizeof(float)));   // colour   (vec3).
            link_attrib(mesh.VBO, 3, 2, GL_FLOAT, sizeof(Vertex), (void *)(9   * sizeof(float)));   // texture  (vec2).
            link_attrib(mesh.VBO, 4, 4, GL_FLOAT, sizeof(Vertex), (void *)(11  * sizeof(float)));   // joints   (vec4).
            link_attrib(mesh.VBO, 5, 4, GL_FLOAT, sizeof(Vertex), (void *)(15  * sizeof(float)));   // weights  (vec4).

            glBindVertexArray(0);                       // unbind VAO.
            glBindBuffer(GL_ARRAY_BUFFER, 0);           // unbind VBO.
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);   // unbind EBO.
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
        tinygltf::Image img = input.images[input.textures[i].source];
        textures.push_back(Texture(img.name, img.width, img.height, img.component, img.bits, img.image.data()));
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
    skins.resize(input.skins.size());

    // loop through each skin in the gltf model.
    for (size_t i = 0; i < input.skins.size(); ++i)
    {
        tinygltf::Skin gltf_skin    = input.skins[i];
        skins[i].name               = gltf_skin.name;
        skins[i].skeleton_root      = node_from_index(gltf_skin.skeleton);

        cout << "skin: " << skins[i].name << " (" << gltf_skin.joints.size() << " joints)" <<  "\n";

        // find nodes that are joints.
        for (int joint_index : gltf_skin.joints)
        { 
            Node *node = node_from_index(joint_index);
            if (node)
            {
                skins[i].joints.push_back(node);
            }
        }

        // get inverse bind matrices from buffer.
        if (gltf_skin.inverseBindMatrices > -1)
        {
            const tinygltf::Accessor &acc       = input.accessors[gltf_skin.inverseBindMatrices];
            const tinygltf::BufferView &view    = input.bufferViews[acc.bufferView];
            const tinygltf::Buffer &buffer      = input.buffers[view.buffer];

            // copy buffer to skin's matrix vector with memcpy.
            skins[i].inverse_bind_matrices.resize(acc.count);
            memcpy(skins[i].inverse_bind_matrices.data(), &buffer.data[acc.byteOffset + view.byteOffset], acc.count * sizeof(glm::mat4));
        }
    }

    // handles loading models without skins.
    if (input.skins.empty())
    {
        Skin skin;
        skins.push_back(skin);
        for (size_t i = 0; i < MAX_JOINTS; ++i)
        {
            skins[0].joint_matrix[i] = glm::mat4(1.0f);
        }
    }
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
    }
}

void Model::update_animations(float delta_time)
{
    if (animations.empty())
    {
        return;
    }

    if (active_animation > static_cast<uint32_t>(animations.size()) - 1)
    {
        cout << "no animation with index: " << active_animation << "\n";
        return;
    }

    Animation &animation = animations[active_animation];

    // increment animation by time variable.
    animation.current_time  += delta_time;
    if (animation.current_time > animation.end)
    {
        animation.current_time -= animation.end;
    }

    for (auto &channel : animation.channels)
    {
        AnimationSampler &sampler = animation.samplers[channel.sampler_index];
        for (size_t i = 0; i < sampler.inputs.size() - 1; ++i)
        {
            if (sampler.interpolation != "LINEAR")
            {
                cout << "only linear interpolations supported atm." << "\n";
                continue;
            }

            // get input keyframes for the current time value.
            if ((animation.current_time >= sampler.inputs[i]) && (animation.current_time <= sampler.inputs[i + 1]))
            {
                float a = (animation.current_time - sampler.inputs[i]) / (sampler.inputs[i + 1] - sampler.inputs[i]);
                if (channel.path == "translation")
                {
                    channel.node->translation = glm::mix(sampler.outputs_vec4[i], sampler.outputs_vec4[i + 1], a);
                }
            
                if (channel.path == "rotation")
                {
                    glm::quat quat_a;
                    quat_a.x = sampler.outputs_vec4[i].x;
                    quat_a.y = sampler.outputs_vec4[i].y;
                    quat_a.z = sampler.outputs_vec4[i].z;
                    quat_a.w = sampler.outputs_vec4[i].w;

                    glm::quat quat_b;
                    quat_b.x = sampler.outputs_vec4[i + 1].x;
                    quat_b.y = sampler.outputs_vec4[i + 1].y;
                    quat_b.z = sampler.outputs_vec4[i + 1].z;
                    quat_b.w = sampler.outputs_vec4[i + 1].w;

                    channel.node->rotation = glm::normalize(glm::slerp(quat_a, quat_b, a));
                }
                if (channel.path == "scale")
                {
                    channel.node->scale = glm::mix(sampler.outputs_vec4[i], sampler.outputs_vec4[i + 1], a);
                }
            }
        }
    }

    // finally, update the joints with the animation.
    for (auto &node : nodes)
    {
        update_joints(node);
    }
}

void Model::update_joints(Node *node)
{
    // if current node has a skin associated with it...
    if (node->skin > -1)
    {
        // get the inverse of the node containing the skin, as it needs to be ignored.
        glm::mat4 inverse_transform = glm::inverse(get_node_matrix(node));

        for (size_t i = 0; i < skins[node->skin].joints.size(); ++i)
        {
            skins[node->skin].joint_matrix[i] = inverse_transform * get_node_matrix(skins[node->skin].joints[i]) * skins[node->skin].inverse_bind_matrices[i];
        }
    }
    for (auto &child : node->children)
    {
        update_joints(child);
    }
}

// load a model from a .gltf file.
Model::Model(std::string filename)
{
    tinygltf::Model glTF_input;         // stores .gltf model reference.
    tinygltf::TinyGLTF glTF_context;    // stores ASCII from file.
    std::string error;                  // outputs warning if fails to load properly.
    std::string warning;                // outputs error if any errors.

    bool loaded = glTF_context.LoadASCIIFromFile(&glTF_input, &error, &warning, MODELS_PATH + filename);
    if (!error.empty())   { cout << "ERR: " << error << "\n"; }
    if (!warning.empty())  { cout << "WARN: " << warning << "\n"; }

    std::vector<uint32_t> index_buffer;
    std::vector<Vertex> vertex_buffer;

    // if loaded correctly, load file contents.
    if (loaded)
    {
        cout << "model: " << filename << "\n";

        // load images, materials, textures.
        load_material(glTF_input);

        //glTF_input.defaultScene
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
    cout << "\n";
}








// probably clean this stuff below up at some point,
// maybe keep the gltf loading/drawing to it's own file?

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
    mesh = Mesh(vertices, indices, 0);
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
    // mesh.draw(GL_LINES, position, rotation, scale, shader, glm::vec3(0.0f));
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
    mesh = Mesh(vertices, indices, 0);
}

// draws grid at centre of world.
void Frustum::draw(glm::vec3 position, Shader shader)
{
    glm::quat rotation  = glm::quat(glm::vec3(0.0f));
    glm::vec3 scale     = glm::vec3(1.0f);

    // draw mesh with GL_LINES.
    // mesh.draw(GL_LINES, position, rotation, scale, shader, glm::vec3(2.0f));
}

// very simple circle for collision shapes. 
Circle::Circle(float radius)
{
    // make vertices along a circle. increment = resolution?
    for (float i = 0; i < 360; ++i)
    {
        Vertex vertex;
        vertex.position = glm::vec3(sinf(glm::radians(i)) * radius, 0.0f, cosf(glm::radians(i)) * radius);
        vertex.normal   = vertex.position;
        vertex.color    = glm::vec3(0.0f);
        vertex.texUV    = glm::vec2(0.0f, 0.0f);

        vertices.push_back(vertex);
        indices.push_back(i);
    }
}

// draw circle.
void Circle::draw(glm::vec3 position, Shader shader, glm::vec3 colour)
{
    glm::quat rotation  = glm::quat(glm::vec3(0.0f));
    glm::vec3 scale     = glm::vec3(1.0f);
    glm::mat4 mvp       = translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);

    // draw mesh with GL_LINES.
    glUseProgram(shader.ID);    // activate the shader being used to draw this model. (potential for multiple shaders per model?)
    glBindVertexArray(VAO);     // bind the VBO with the vertexes from the mesh.
    
    // per mesh shader updates; colour and combined matrix transformation.
    glUniform3fv(glGetUniformLocation(shader.ID, "albedo"), 1, glm::value_ptr(colour));                     // base colour of mesh.
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "mvp"), 1, GL_FALSE, glm::value_ptr(mvp));           // mesh view projection matrix.

    // set polygon mode and then draw elements.
    glPolygonMode(GL_FRONT_AND_BACK, shader.mode);
    glLineWidth(1.0f);
    glDrawElements(GL_LINE_LOOP, indices.size() * sizeof(indices[0]), GL_UNSIGNED_INT, 0);

    // unbind vertex array and texture.
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
    link_attrib(VBO, 0, 2, GL_FLOAT, 4 * sizeof(float), (void*)(0));
    link_attrib(VBO, 1, 2, GL_FLOAT, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    
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

    // unbind vertex array and texture.
    glBindTexture(GL_TEXTURE_2D, 0);
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
    link_attrib(VBO, 0, 2, GL_FLOAT, 4 * sizeof(float), (void*)(0));
    link_attrib(VBO, 1, 2, GL_FLOAT, 4 * sizeof(float), (void*)(2 * sizeof(float)));

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

Mesh::Mesh(std::vector<Vertex> &vertices, std::vector<GLuint> &indices, int type)
{
    // set local mesh vectors to input for use in draw function.
    Mesh::vertices  = vertices;
	Mesh::indices   = indices;
    Mesh::type      = type;

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

    // link vertex attributes to shader layouts. first arg = layout number in the shader.
    link_attrib(VBO, 0, 3, GL_FLOAT, sizeof(Vertex), (void*)(0));   // position (vec3).
    link_attrib(VBO, 1, 3, GL_FLOAT, sizeof(Vertex), (void*)(3   * sizeof(float)));   // normal (vec3).
    link_attrib(VBO, 2, 3, GL_FLOAT, sizeof(Vertex), (void*)(6   * sizeof(float)));   // colour (vec3).
    link_attrib(VBO, 3, 2, GL_FLOAT, sizeof(Vertex), (void*)(9   * sizeof(float)));   // texture (vec2).
    link_attrib(VBO, 4, 4, GL_FLOAT, sizeof(Vertex), (void*)(11  * sizeof(float)));   // joints (vec4).
    link_attrib(VBO, 5, 4, GL_FLOAT, sizeof(Vertex), (void*)(15  * sizeof(float)));   // weights (vec4).

    glBindVertexArray(0);                       // unbind VAO.
    glBindBuffer(GL_ARRAY_BUFFER, 0);           // unbind VBO.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);   // unbind EBO.
}