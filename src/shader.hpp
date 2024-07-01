#pragma once

#include <glad.h>   // ID and mode type.
#include <string>   // load vert and frag as string.

#define SHADER_PATH "./assets/shaders/"

class Shader
{
    public:
        GLuint ID;
        GLenum mode;
        Shader(GLenum mode, std::string vert_file, std::string frag_file);
    private:
        void compile_errors(unsigned int shader, const char* type);
};