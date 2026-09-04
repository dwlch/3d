#pragma once

#include <vector>       // vertices etc.
#include <array>        // used in EPA to store faces and edges.
#include <memory>       // used in level loading.
#include "glm/gtx/quaternion.hpp"
#include "glm/glm.hpp"

// internal
#include "utility.hpp"
#include "draw.hpp"

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
    virtual ~Collider() {};
    virtual glm::vec3 furthest_point(glm::vec3 direction) const = 0;
    virtual void draw(int shader, const Camera &camera) = 0;

    enum Type
    {
        SOLID,
        WARP,
        BOUNCE
    };
    
    // bool is_trigger     = false;
    // int type            = 0;


    Type type           = Type::SOLID;
    int target_level    = 0;
    glm::vec3 spawn     = glm::vec3(0.0f);
    std::pair<glm::vec3, glm::vec3> AABB;
};

// cylinder collision shape. defined using height, radius, position, and axis.
struct CylinderCollider : public Collider
{
    glm::vec3 position;
    glm::vec3 axis;
    glm::vec3 colour = glm::vec3(5.0f);
    float height;
    float radius;
    Circle circle{radius};
    
    // constructors.
    CylinderCollider() : position(glm::vec3(0.0f)), axis(glm::vec3(0.0f, 1.0f, 0.0f)), height(0.0f), radius(1.0f), circle(Circle(1.0f)) {}
    CylinderCollider(glm::vec3 position, glm::vec3 axis, float height, float radius) : position(position), axis(axis), height(height), radius(radius), circle(Circle(radius))
    {
        // AABB.first      = glm::vec3(position.x - radius, position.y - radius, position.z);
        // AABB.second     = glm::vec3(position.x + radius, position.y + radius, position.z + height);

        // glm::vec3 half_size{ radius, height,  radius };
        // AABB.first      = position - half_size;
        // AABB.second     = position + half_size;

        AABB.first      = position + glm::vec3(-radius, -height, -radius);
        AABB.second     = position + glm::vec3( radius,  height,  radius);
    }
    ~CylinderCollider() override {};

    void move(const glm::vec3& new_position)
    {
        position        = new_position;
        // glm::vec3 half_size{ radius, height,  radius };
        // AABB.first      = position - half_size;
        // AABB.second     = position + half_size;

        AABB.first      = new_position + glm::vec3(-radius, -height, -radius);
        AABB.second     = new_position + glm::vec3( radius,  height,  radius);
    }

    // draws 2 circles, one at base of collider one at the top, using radius.
    void draw(int shader, const Camera &camera) override
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

// arbitrary convex mesh collision shape. defined using a vector of vertices.
// not sure if this ever requires indices to get correct order or doesn't matter.
struct MeshCollider : public Collider
{
    std::vector<glm::vec3> vertices; // stores collision shape.

    // default constructor.
    MeshCollider(std::vector<glm::vec3> &vertices, const glm::vec3 &min, const glm::vec3 &max) : vertices(vertices)
    {
        AABB.first  = min;
        AABB.second = max;
    }
    ~MeshCollider() override {};

    void draw(int shader, const Camera &camera) override {};

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
Collision get_collision(const Collider *a, const Collider *b);

// returns true if the AABB of both colliders intersect.
bool AABB_check(const Collider *a, const Collider *b);