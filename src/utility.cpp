#include "utility.hpp"
#include <fstream>      // get_file_contents().

// linear interpolation between two floats
float lerp_float(float start, float end, float time)
{
    return start + time * (end - start);
}

// linear interpolation between two vec2s
glm::vec2 lerp_vec2(glm::vec2 a, glm::vec2 b, float time)
{
    glm::vec2 output = glm::vec2(0.0f);

    output.x = a.x + time * (b.x - a.x);
    output.y = a.y + time * (b.y - a.y);

    return output;
}

bool is_vec2_zero(const glm::vec2 a)
{
    bool output = (a.x == 0.0f) && (a.y == 0.0f);
    return output;
}


bool vec2_equals(glm::vec2 a, glm::vec2 b)
{
    float deltaX = fabs(a.x - b.x);
    float deltaY = fabs(a.y - b.y);

    return deltaX < EPSILON && deltaY < EPSILON;
}

// vec3 functions.
// linear interpolation between two vec2s
glm::vec3 lerp_vec3(glm::vec3 a, glm::vec3 b, float time)
{
    glm::vec3 output = glm::vec3(0.0f);

    output.x = a.x + time * ( b.x - a.x );
    output.y = a.y + time * ( b.y - a.y );
    output.y = a.z + time * ( b.z - a.z );

    return output;
}

// return true if input vec3 are in the same direction.
bool same_direction(const glm::vec3 &a, const glm::vec3 &b)
{
    return glm::dot(a, b) > 0.0f;
}

bool is_vec3_zero(const glm::vec3 a)
{
    bool output = (a.x == 0.0f) && (a.y == 0.0f) && (a.z == 0.0f);
    return output;
}

bool vec3_equals(const glm::vec3 a, const glm::vec3 b)
{
    float deltaX = fabs(a.x - b.x);
    float deltaY = fabs(a.y - b.y);
    float deltaZ = fabs(a.z - b.z);

    return deltaX < EPSILON && deltaY < EPSILON && deltaZ < EPSILON;
}

// IO stuff.
// reads a text file and outputs a string with everything in the text file
std::string get_file_contents(const char *filename)
{
    std::ifstream read(filename, std::ios::binary);
    
    if (read)
    {
        std::string contents;
        read.seekg(0, std::ios::end);
        contents.resize(read.tellg());
        read.seekg(0, std::ios::beg);
        read.read(&contents[0], contents.size());
        read.close();
        return contents;
    }
    throw errno;
}