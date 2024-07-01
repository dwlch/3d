#include "collision.hpp"
#include <iostream>     // file loading.

using std::cout;
using glm::vec3;

// change it so the collider is swept across it's movement vector to get a new shape? 
glm::vec3 Support(const Collider *a, const Collider *b, glm::vec3 direction)
{
    return a->furthest_point(direction) - b->furthest_point(-direction);
}

bool line(std::vector<glm::vec3> &simplex, glm::vec3 &direction)
{
    // a is always the point that has just been added (end of the vector).
    // b cannot be the closest point as it is the already existing point in the simplex.
    // so we know a is always closer.
    vec3 a  = simplex[1];
    vec3 b  = simplex[0];
    
    vec3 ab = b - a;
    vec3 ao =   - a;

    if (same_direction(ab, ao))
    {
        simplex     = {a, b};
        direction   = glm::cross(glm::cross(ab, ao), ab);
    }
    else
    {
        simplex     = {a};
        direction   = ao;
    }

    // collision returns false in every case except tetrahedron.
    return false;
}

bool triangle(std::vector<glm::vec3> &simplex, glm::vec3 &direction)
{
    // a is the new point.
    vec3 a      = simplex[2];
    vec3 b      = simplex[1];
    vec3 c      = simplex[0];

    vec3 ab     = b - a;
    vec3 ac     = c - a;
    vec3 ao     =   - a;
    vec3 abc    = glm::cross(ab, ac);

    if (same_direction(glm::cross(abc, ac), ao))
    {
        if (same_direction(ac, ao))
        {
            simplex     = {a, c};
            direction   = glm::cross(glm::cross(ac, ao), ac);
        }
        else
        {
            // if same_direction(ab, ao) check.
            return line(simplex, direction);
        }
    }
    else if (same_direction(cross(ab, abc), ao))
    {
        // if same_direction(ab, ao) check.
        return line(simplex, direction);
    }
    else
    {
        if (same_direction(abc, ao))
        {
            simplex     = {b, c, a};
            direction   = abc;
        }
        else
        {
            // simplex     = {c, b, a};
            direction   = -abc;
        }
    }

    // collision returns false in every case except tetrahedron.
    return false;
}

bool tetrahedron(std::vector<glm::vec3> &simplex, glm::vec3 &direction)
{
    vec3 a  = simplex[3];
    vec3 b  = simplex[2];
    vec3 c  = simplex[1];
    vec3 d  = simplex[0];

    vec3 ab = b - a;
    vec3 ac = c - a;
    vec3 ad = d - a;
    vec3 ao =   - a;

    vec3 acb = glm::cross(ac, ab);
    vec3 adc = glm::cross(ad, ac);
    vec3 abd = glm::cross(ab, ad);

    // determine which face is closest to check rather than a full tetrahedron check.
    if (same_direction(acb, ao))
    {
        direction = acb;
        return triangle(simplex = {a, c, b}, direction);
    }
    else if (same_direction(adc, ao))
    {
        direction = adc;
        return triangle(simplex = {a, d, c}, direction);
    }
    else if (same_direction(abd, ao))
    {
        direction = abd;
        return triangle(simplex = {a, b, d}, direction);
    }
    else
    {
        // contains the origin, so returns true.
        return true;
    }
}

// determine simplex case to query based on the number of points.
bool do_simplex(std::vector<glm::vec3> &simplex, glm::vec3 &direction)
{   
    switch (simplex.size())
    {
        case 2: return line(simplex, direction);
        case 3: return triangle(simplex, direction);
        case 4: return tetrahedron(simplex, direction);
        default : assert(false);
    }
    return false;
}

// boolean GJK function that returns true if two colliders intersect.
bool GJK(const Collider *a, const Collider *b, std::vector<glm::vec3> &simplex)
{
    // start with any point in the minkowski difference,
    // can either be arbitray (1,0,0) or between the two colliders positions (faster?).
    glm::vec3 direction = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 support   = Support(a, b, direction);

    // if support is the origin, no intersection?
    if (is_vec3_zero(support))
    {
        return false;
    }

    simplex.push_back(support); // use the support as the first point in the simplex.
    direction = -support;       // begin with the direction from the simplex towards the origin.

    // rather than using while (true), this prevents infinite loops when dealing with curved surfaces.
    for (unsigned int i = 0; i < GJK_MAX_ITERATIONS; i++)
    {
        support = Support(a, b, direction);         // get a new support point using the current search direction.
        if (glm::dot(support, direction) < 0.0f)    // no intersection if origin is beyond support point.
        {
            break; 
        }
        simplex.push_back(support);                 // insert new support into simplex.
        if(do_simplex(simplex, direction))          // returns true if simplex contains the origin in the direction given.
        {
            return true;
        } 
    }
    return false;
}

// EPA stuff begins here.
void add_to_polytope(Polytope &polytope, const Face face)
{    
    assert(polytope.face_count < EPA_MAX_FACES);

    // add face to array and increase face count.
    polytope.faces[polytope.face_count] = face;
    polytope.face_count++;
}

// gets the face inside the polytope that is closest to the origin.
Face get_closest_face(const Polytope &polytope)
{
    Face closest_face       = polytope.faces[0];
    closest_face.distance   = FLT_MAX;
    closest_face.normal     = closest_face.normal;

    // check every face in the polytope.
    for (size_t i = 0; i < polytope.face_count; i++)
    {
        // get a new face to compare against closest face.
        Face face       = polytope.faces[i];
        face.distance   = glm::dot(face.point[0], face.normal);

        // if new face is closer than current face, set it as the closest face.
        if (face.distance < closest_face.distance)
        {
            closest_face = face;
        }
    }

    assert(closest_face.distance < FLT_MAX);
    return closest_face;
}

// EPA (expanding polytope algorithm)
// references:  https://github.com/ClysmiC/Cataclysm/blob/master/code/Gjk.cpp
//              https://github.com/kevinmoran/GJK/blob/master/GJK.h
//              https://github.com/Another-Ghost/3D-Collision-Detection-and-Resolution-Using-GJK-and-EPA/blob/master/CSC8503/CSC8503Common/GJK.cpp
Results EPA(const Collider *collider_a, const Collider *collider_b, std::vector<glm::vec3> &simplex)
{
    // create a polytope from the simplex we got from succesful GJK intersection.
    vec3 a = simplex[3];
    vec3 b = simplex[2];
    vec3 c = simplex[1];
    vec3 d = simplex[0];

    Polytope polytope;
    add_to_polytope(polytope, Face(a, c, b));
    add_to_polytope(polytope, Face(a, d, c));
    add_to_polytope(polytope, Face(a, b, d));
    add_to_polytope(polytope, Face(b, c, d));

    
    Results results;    // combined angle of collision * depth.
    Face closest_face;  // store most recent closest face.

    for (unsigned int i = 0; i < EPA_MAX_ITERATIONS; i++)
    {
        closest_face    = get_closest_face(polytope);                           // we get the closest face on the polytope.
        vec3 support    = Support(collider_a, collider_b, closest_face.normal); // get support in direction of closest face.
        float distance  = glm::dot(support, closest_face.normal);               // get how far away the face is from the support point.

        // if the closest face is within than the distance margin:
        if (distance - closest_face.distance < EPA_ACCURACY)
        {
            results.normal   = closest_face.normal;
            results.depth    = distance + EPA_ACCURACY;
            return results;
        }

        // face is not sufficiently close enough, so expand polytope in support direction.
        // store edges of removed faces in this array.
        std::array<std::pair<vec3, vec3>, EPA_MAX_EDGES> edges;
        int edges_count = 0;

        // find all triangles that are facing the support.
        for (size_t i = 0; i < polytope.face_count; i++)
        {
            // check if the face is 'visible' from support, if so needs to be removed.
            Face face = polytope.faces[i];
            if (same_direction(face.normal, (support - face.point[0])))
            {
                // add edges of removed face to array, then remove any duplicate edges.
                for (int j = 0; j < 3; j++)
                {
                    std::pair<vec3, vec3> edge  = std::pair<vec3, vec3>(face.point[j], face.point[(j + 1) % 3]);
                    bool is_duplicate           = false;

                    // loop through every edge in the edges array.
                    for (int k = 0; k < edges_count; k++)
                    {
                        // if both points in edge i equal the edge we are checking, remove it.
                        if (vec3_equals(edges[k].first, edge.second) && vec3_equals(edges[k].second, edge.first))
                        {
                            is_duplicate    = true;
                            edges[k]        = edges[edges_count - 1];
                            edges_count     --;
                            k               = edges_count;
                        }
                    }

                    // if edge is not duplicate, ie is unique, add it to the edges array.
                    if (!is_duplicate)
                    {
                        if(edges_count >= EPA_MAX_EDGES) break;
                        edges[edges_count] = edge;
                        edges_count++;
                    }
                }

                // remove the current face from the polytope, reduce i by 1;
                polytope.faces[i] = polytope.faces[polytope.face_count - 1];
                polytope.face_count--;
                i--;
            }
        }

        // finally, reconstruct polytope with support point.
        // the new face is always the 2 points on the edge to add, and the expand point (support).
        for(int i = 0; i < edges_count; i++)
        {
            if(polytope.face_count >= EPA_MAX_FACES) break;
            Face face = Face(edges[i].first, edges[i].second, support);
            
            // maintain CCW winding by checking if normal is incorrect.
            float bias = 0.000001f;
            if (glm::dot(face.point[0], face.normal) + bias < 0.0f)
            {
                polytope.faces[polytope.face_count].point[0]    = face.point[1];
                polytope.faces[polytope.face_count].point[1]    = face.point[0];
                polytope.faces[polytope.face_count].normal      = -face.normal;
            }
            else
            {
                polytope.faces[polytope.face_count] = face;
            }
            polytope.face_count++;
        }
    }

    std::cout << "EPA did not converge \n";
    results.normal   = closest_face.normal;
    results.depth    = glm::dot(closest_face.point[0], closest_face.normal) + EPA_ACCURACY;
    return results;
}

// this function actually does the GJK check + EPA, and returns the values as a Results struct
Results is_collision(const Collider *a, const Collider *b)
{
    Results test;                   // stores the info from collision test.
    std::vector<glm::vec3> simplex; // simplex constructed in GJK step, iterated on in EPA to get penetration.
    simplex.reserve(4);

    if (GJK(a, b, simplex))
    {
        test            = EPA(a, b, simplex);
        test.collided   = true;
    }
    else 
    {
        test.collided   = false;
    }
    return test;
}

// return vector of colliders from given level gltf file.
void get_colliders_from_level(Model &level, std::vector<std::unique_ptr<Collider>> &colliders)
{
    colliders.clear();
    for (size_t i = 0; i < level.meshes.size(); i++)
    {
        if (level.meshes[i].collider)
        {
            colliders.push_back(std::move(std::unique_ptr<Collider>(new MeshCollider(level.meshes[i]))));
        }  
    }
    std::cout << "level loaded!" << "\n\n";
}