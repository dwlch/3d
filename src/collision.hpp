#pragma once

#include <vector>       // vertices etc.
#include <array>        // used in EPA to store faces and edges.
#include <memory>       // used in level loading.
#include "glm/gtx/quaternion.hpp"
#include "glm/glm.hpp"

// #include "camera.hpp"   // drawing requires camera.
#include "draw.hpp"
#include "utility.hpp"

#define GJK_MAX_ITERATIONS 128                  // limit of GJK iterations.
#define EPA_MAX_ITERATIONS 255                  // limit of EPA iterations.
#define EPA_ACCURACY 0.001f                     // 'close enough' margin. 0.001f 100% safe, 0.0001f safe i think.
const int EPA_MAX_EDGES = 128;                  // allocates space in edge array.
const int EPA_MAX_FACES = EPA_MAX_EDGES * 2;    // allocates space in face array.

// stores the resulting information from a collision.
struct Collision
{
    glm::vec3 normal;   // angle of resolved collision.
    float depth;        // depth of collision response.
    bool collided;      // if it is actually a collision or not.
    bool is_trigger;    //
};

// store faces inside the polytope.
struct Face
{
    glm::vec3 normal;
    float distance      = FLT_MAX;
    std::array<glm::vec3, 3> point;

    // constructors.
    Face()
    {
        point[0] = glm::vec3(0.0f);
        point[1] = glm::vec3(0.0f);
        point[2] = glm::vec3(0.0f);
        Face::normal = glm::vec3(0.0f);
    }
    Face(glm::vec3 a, glm::vec3 b, glm::vec3 c)
    {
        point[0] = a;
        point[1] = b;
        point[2] = c;
        glm::vec3 ab = point[1] - point[0];
        glm::vec3 ac = point[2] - point[0];
        Face::normal = glm::normalize(glm::cross(ab, ac));
    }
};

struct Polytope
{
    std::array<Face, EPA_MAX_FACES> faces;
    size_t face_count = 0;
};

// abstract parent collider.
struct Collider
{
    bool is_trigger;
    glm::vec3 colour;
    int target_level    = 0;
    glm::vec3 spawn     = glm::vec3(0.0f);  // spawn point for level changes.

    virtual glm::vec3 furthest_point(glm::vec3 direction) const = 0;
    virtual void draw(const Shader &shader, const Camera &camera) = 0;
};

// cylinder collision shape. defined using height, radius, position, and axis.
struct CylinderCollider : public Collider
{
    glm::vec3 colour = glm::vec3(5.0f);
    glm::vec3 position;
    glm::vec3 postion_next_frame;
    glm::vec3 axis;
    float height;
    float radius;
    Circle circle{radius};

    // constructors.
    CylinderCollider() : position(glm::vec3(0.0f)), axis(glm::vec3(0.0f, 1.0f, 0.0f)), height(0.0f), radius(1.0f), circle(Circle(1.0f)) {}
    CylinderCollider(glm::vec3 position, glm::vec3 axis, float height, float radius) : position(position), axis(axis), height(height), radius(radius), circle(Circle(radius)) {}

    // draws 2 circles, one at base of collider one at the top, using radius.
    void draw(const Shader &shader, const Camera &camera) override
    {
        circle.draw(position, shader, camera, colour);
        circle.draw(glm::vec3(position.x, position.y + height, position.z), shader, camera, colour);
    }

    // support mapping from http://www.cs.kent.edu/~ruttan/GameEngines/lectures/gjk1.pdf
    glm::vec3 furthest_point(glm::vec3 direction) const override
    {   
        float half_height   = height / 2;
        glm::vec3 centre    = glm::vec3(position.x, position.y + half_height, position.z);
        glm::vec3 w         = direction - (glm::dot(axis, direction) * axis);
        glm::vec3 point     = glm::vec3(0.0f);

        if (!is_vec3_zero(w))
        {
            point = centre + (glm::sign(glm::dot(axis, direction)) * (half_height * axis)) + (radius * (w / glm::length(w)));
        }
        else 
        {
            point = centre + (glm::sign(glm::dot(axis, direction)) * (half_height * axis));
        }
        return point;
    }
};

// cylinder collision shape. defined using height, radius, position, and axis.
struct RayCollider : public Collider
{
    glm::vec3 colour = glm::vec3(0.0f);
    glm::vec3 point_a;
    glm::vec3 point_b;


    // constructors.
    RayCollider() : point_a(glm::vec3(0.0f)), point_b(glm::vec3(1.0f)) {}
    RayCollider(glm::vec3 point_a, glm::vec3 point_b) : point_a(point_a), point_b(point_b) {}

    // draws 2 circles, one at base of collider one at the top, using radius.
    void draw(const Shader &shader, const Camera &camera) override
    {

    }

    // support mapping from http://www.cs.kent.edu/~ruttan/GameEngines/lectures/gjk1.pdf
    glm::vec3 furthest_point(glm::vec3 direction) const override
    {   

        glm::vec3 point     = glm::vec3(0.0f);

        return point;
    }
};

// arbitrary convex mesh collision shape. defined using a vector of vertices.
// not sure if this ever requires indices to get correct order or doesn't matter.
struct MeshCollider : public Collider
{
    glm::vec3 colour = glm::vec3(0.9f, 0.5f, 0.3f);
    std::vector<glm::vec3> vertices;
    // bool is_trigger;
    // MeshPrimitive mesh;

    // default constructor.
    MeshCollider(std::vector<glm::vec3> &vertices) : vertices(vertices) 
    {
        // bind_buffers(mesh.VAO, mesh.VBO, mesh.EBO, vertices, )
    }

    // draw mesh wrapper, simply draws the mesh used for mesh collider input.
    void draw(const Shader &shader, const Camera &camera) override
    {

        // mesh.draw(GL_TRIANGLES, glm::vec3(0.0f), glm::quat(glm::vec3(0.0f)), glm::vec3(1.0f), shader, colour);
    }

    // loop through each vertex position,
    // get the vertex in the mesh that is furthest in direction.
    glm::vec3 furthest_point(glm::vec3 direction) const
    {
        glm::vec3 point         = glm::vec3(0.0f);;
        float furthest_distance = -FLT_MAX;

        for (size_t i = 0; i < vertices.size(); ++i)
        {
            float distance = glm::dot(vertices[i], direction);
            if (distance > furthest_distance)
            {
                furthest_distance   = distance;
                point               = vertices[i];
            }
        }
        return point;
    };
};

// call this function to query a collision between any two collider shapes.
Collision is_collision(const Collider *a, const Collider *b);