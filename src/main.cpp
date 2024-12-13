/*
opengl simple game engine.
version start date : 22 08 2022.
*/

// c++ includes.
#include <iostream>
#include <vector>
#include <array>        // for shader array.
#include <memory>       // for collision array.

// GL includes.
#include "glad.h"
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include <glm/gtc/type_ptr.hpp>

// internal includes.
#include "defines.hpp"
#include "camera.hpp"
#include "shader.hpp"
#include "draw.hpp"
#include "collision.hpp"
#include "player.hpp"
#include "input.hpp"
#include "shadowmap.hpp"

// plan to use this enum to toggle level editor.
enum Mode
{
    GAME,
    EDIT
};

Mode mode = Mode::GAME;

int main(void)
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);                  // state which version of OpenGL is in use,
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);                  // in this case version 3.3 (major 3, minor 3).
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);                     // prevents window from being resized.
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // state that the CORE profile is in use.
	
    // create glfw window at the centre of the primary monitor.
    GLFWwindow* window      = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "opengl", NULL, NULL);
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    glfwSetWindowPos(window, (mode->width/2) - (WINDOW_WIDTH/2), (mode->height/2) - (WINDOW_HEIGHT/2));
    
    // checks if window creation failed, terminates glfw if so.
    if (!window)
    {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);                             // makes the created window the 'OpenGL context'.
    glfwSwapInterval(1);                                        // enable vsync.
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);  // set mouse mode here (for now using normal mode until i re-add mouse input).
    glfwSetKeyCallback(window, key_callback);                   // set the key callback to input.hpp's key_callback function.
    
    gladLoadGL();
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);  // define the OpenGL viewport in the window.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);           // specify the screen clear colour (RGBA).
    glEnable(GL_DEPTH_TEST);                        // enables depth test so tris get drawn in correct order.
    glEnable(GL_DEPTH_CLAMP);                       // depth clipping?
    glEnable(GL_CULL_FACE);                         // enable face culling.
    glEnable(GL_POLYGON_OFFSET_FILL);               // "slope scale depth bias".
    glEnable(GL_MULTISAMPLE);
    glDepthFunc(GL_LESS);                           // GL_LESS passes if new depth value is LESS than the stored depth value.
    glCullFace(GL_BACK);                            // cull back faces.
    glCullFace(GL_CCW);                             // counter clockwise indice order.
    
    // can probs trim these down, dont want to do any fancy shaders anymore rlly, just shadows.
    std::array<Shader, SHADER_COUNT> shader = {                     // array of all shaders, 0 is always screen/post-process shader.
        Shader(GL_FILL, "framebuffer.vert", "framebuffer.frag"),    // post process framebuffer shader.
        Shader(GL_FILL, "shadow_map.vert",  "shadow_map.frag"),     // shadowmap shader.
        Shader(GL_FILL, "default.vert",     "default.frag"),        // default material shader.
        Shader(GL_FILL, "cel.vert",         "cel.frag"),            // character model shader.
        Shader(GL_LINE, "default.vert",     "line.frag")            // wireframe shader.
    };

    // array of levels, which are just gltf files.
    // should probably do it recursively like "scene_" & i etc.
    std::array<Model, LEVEL_COUNT> level = {
        Model("scene_0000.gltf"),
        Model("scene_0001.gltf")
    };

    // vector array of any colliders to test against player in update.
    // contents rewritten on level load etc.
    std::vector<std::unique_ptr<Collider>> colliders;
    int current_level = 0;
    get_colliders_from_level(level[current_level], colliders);

    // mandatory inits.
    Player player;
    Camera camera(SHADOWMAP_SIZE);
    ShadowMap shadowmap(SHADOWMAP_SIZE);
    ScreenTexture screen;
    
    // maybe should make a 'light' struct that has these properties?
    glm::vec3 light_pos         = glm::vec3(0.7f, 1.0f, 0.3f);
    glm::vec3 light_target      = glm::vec3(0.0f);
    float dt                    = 0.01f;

    while(!glfwWindowShouldClose(window))
    {
        // update step.
        {
            camera.get_input();                     // camera input, calculates camera orientation vec3.
            player.update(dt, colliders, camera);   // player input and movement, sent a vector of colliders.
            camera.update(player.camera_lookat);    // update camera matrix using target position.
        }

        // draw step.
        {
            // shadowmap pass. could this be all moved into shadowmap.cpp? 
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glViewport(0, 0, shadowmap.size, shadowmap.size);
            glUseProgram(shader[SHADER_SHADOWMAP].ID);
            glPolygonOffset(6.0f, 1.0f); // factor, unit.
            
            // get shadowmap cascade matrices.
            glm::vec3 light_direction = glm::vec3(glm::normalize(light_pos - light_target));
            shadowmap.get_light_projection(camera, light_direction);

            // do for each cascade in the shadowmap array.
            for (int i = 0; i < NUM_CASCADES; ++i)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, shadowmap.FBO);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowmap.depth_maps[i], 0);
                glUniformMatrix4fv(glGetUniformLocation(shader[SHADER_SHADOWMAP].ID, "light"), 1, GL_FALSE, glm::value_ptr(shadowmap.cascade_proj[i]));
                glClear(GL_DEPTH_BUFFER_BIT);       

                // render geometry to shadow map.
                player.draw(shader[SHADER_SHADOWMAP], shader[SHADER_SHADOWMAP]);
                level[current_level].draw(glm::vec3(0.0f), glm::quat(glm::vec3(0.0f)), glm::vec3(1.0f), shader[SHADER_SHADOWMAP], glm::vec3(1.0f));
            }

            // render pass.
            glPolygonOffset(0, 0);
            glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
            glBindFramebuffer(GL_FRAMEBUFFER, screen.FBO);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            // draw scene to post-process framebuffer with shadows applied.
            // can probably move the shadowmap update stuff into the draw function itself, as this always happens when a draw occurs anyway?
            shadowmap.update_uniforms(shader[SHADER_CEL], camera, light_pos);
            player.draw(shader[SHADER_CEL], shader[SHADER_LINE]);

            shadowmap.update_uniforms(shader[SHADER_DEFAULT], camera, light_pos);
            level[current_level].draw(glm::vec3(0.0f), glm::quat(glm::vec3(0.0f)), glm::vec3(1.0f), shader[SHADER_DEFAULT], glm::vec3(1.0f));

            // finally, draw the screen framebuffer.
            screen.draw(shader[SHADER_FRAMEBUFFER]);
        }

        // swap the back buffer with the front buffer + poll IO events.
		glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // on exit.
    // loop through all shaders and delete each one.
    for (size_t i = 0; i < shader.size(); ++i)
    {
        glDeleteProgram(shader[i].ID);
    }
    
	glfwDestroyWindow(window);  // destroy window prior to ending program.
	glfwTerminate();            // end GLFW.
	return 0;
}