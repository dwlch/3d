#pragma once

#include <glad.h>   // ID and mode type.
#include <string>   // load vert and frag as string.
#include <iostream>

#include "defines.hpp"

struct Shader
{
    GLuint ID;
    GLenum mode;
    Shader(GLenum mode, std::string vert_file, std::string frag_file);
    void compile_errors(unsigned int shader, const char* type);

    ~Shader()
    {
        glDeleteProgram(this->ID);
        std::cout << "Shader with ID " << ID << " deleted!\n";
    }
};