#pragma once

#include <string>
#include <glm/glm.hpp>

#define EPSILON 0.0001f     // compare floats vs this.

// float.
float lerp_float(float start, float end, float time);

// vec2.
glm::vec2 lerp_vec2(glm::vec2 a, glm::vec2 b, float time);
bool is_vec2_zero(const glm::vec2 a);
bool vec2_equals(glm::vec2 a, glm::vec2 b);

// vec3.
glm::vec3 lerp_vec3(glm::vec3 a, glm::vec3 b, float time);
bool same_direction(const glm::vec3 &a, const glm::vec3 &b);
bool is_vec3_zero(const glm::vec3 a);
bool vec3_equals(const glm::vec3 a, const glm::vec3 b);

// IO stuff.
std::string get_file_contents(const char *filename);