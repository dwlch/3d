#include "shader.hpp"
#include "utility.hpp"  // get_file_contents() function.
#include <iostream>     // std::cout.
#include <cerrno>       // error checking.
#include <cstring>      // for error compile.

Shader::Shader(const GLenum mode, std::string vert_file, std::string frag_file)
{
    // sets draw mode (GL_FILL, GL_LINE etc).
    Shader::mode = mode;

    // get contents of vert and frag files as a string.
    std::string vert_code   = get_file_contents((SHADER_PATH + vert_file).data());
    std::string frag_code   = get_file_contents((SHADER_PATH + frag_file).data());

    // converts the strings into character arrays.
    const char *vert_source = vert_code.data();
    const char *frag_source = frag_code.data();

    // create vertex shader.
    GLuint vert_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_shader, 1, &vert_source, NULL);
    glCompileShader(vert_shader);
    compile_errors(vert_shader, "vert");

    // create fragment shader.
    GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_shader, 1, &frag_source, NULL);
    glCompileShader(frag_shader);
    compile_errors(frag_shader, "frag");
    
    ID = glCreateProgram();             // create program shader.
    glAttachShader(ID, vert_shader);    // attach vertex shader.
    glAttachShader(ID, frag_shader);    // fragment shader.
    glLinkProgram(ID);                  // link all the shaders together into the shader program.
    compile_errors(ID, "program");      // check if shader program compiled correctly.

    // delete the vert + frag shaders.
    // they have been linked to the program shader so no longer needed.
    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);
}

void Shader::compile_errors(unsigned int shader, const char* type)
{
    GLint compiled;
    char info_log[1024];

    // determine error check based on type (shader or program).
    if (std::strcmp(type, "program") != 0)
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        glGetShaderInfoLog(shader, sizeof(info_log), NULL, info_log);
        if (compiled == GL_FALSE)
        {
            std::cout << "shader compilation error for type: " << type << "\n";
        }
        else
        {
            std::cout << "shader compiled: " << type << "\n";
        }
    }
    else
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &compiled);
        glGetProgramInfoLog(shader, sizeof(info_log), NULL, info_log);
        if (compiled == GL_FALSE)
        {
            std::cout << "shader linking error for type: " << type << "\n";
        }
        else
        {
            std::cout << "shader linked: " << type << "\n\n";
        }
    }
}