/*
opengl simple game engine.
version start date : 22 08 2022.
*/

// c++ includes.
#include <iostream>     // console printing.
#include <array>        // shader array.
#include <chrono>       // needed for timestep.

// GL includes.
#include "glad.h"
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

// internal includes.
#include "defines.hpp"      // global variables.
#include "camera.hpp"       // camera + shadowmap.
#include "shader.hpp"       // shader loading.
#include "draw.hpp"         // gltf model loading + some primitive mesh stuff.
#include "collision.hpp"    // gjk + epa collision implementation, also level loading.
#include "player.hpp"       // player controller.
#include "npc.hpp"          // npcs. (might factor some of this elsewhere).
#include "level.hpp"        // handles level loading.
#include "input.hpp"        // input handler (needs some work).

// plan to use this enum to toggle level editor.
enum Mode
{
    GAME,
    EDIT
};

// global stuff.
Mode mode   = Mode::GAME;
bool debug  = false;

void update(Player &player, Camera &camera, Level &level, double dt)
{
    // basically anything that moves needs the dt value:
    // player position (xy movement, jumping).
    // camera pitch and yaw.
    // also all lerp values need dt as well? ig bcos they constitute the final term,
    // but u dont want to * dt the entire value because that would get rid of the lerp i think?

    for (size_t i = 0; i < level.npcs.size(); ++i)
    {
        level.npcs[i].update(dt);
    }
    camera.get_input(dt);                   // camera input, calculates camera orientation vec3.
    player.update(dt, level, camera);       // player input and movement, sent a vector of colliders.
    camera.update(player.camera_lookat);    // update camera matrix using target position.
    update_inputs();
}

void draw(Camera camera, ScreenTexture screen, std::array<Shader, SHADER_COUNT> shader, Player player, Level &level)
{
    // draw to shadowmaps.
    glViewport(0, 0, SHADOWMAP_SIZE, SHADOWMAP_SIZE);
    glPolygonOffset(6.0f, 1.0f); // factor, unit.
    glBindFramebuffer(GL_FRAMEBUFFER, camera.FBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // this could be organised/factored better.
    // the prob is that you need to call the draw functions inside this, and i dont want to have to pass the models to the camera etc.
    // but does it need to be inside the camera? maybe this should just be in the draw file, but then it's already got so much stuff...
    camera.get_cascades();
    glUseProgram(shader[SHADER_SHADOWMAP].ID);

    // do for each cascade in the shadowmap array.
    for (int i = 0; i < NUM_CASCADES; ++i)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, camera.depth_maps[i], 0);
        glUniformMatrix4fv(glGetUniformLocation(shader[SHADER_SHADOWMAP].ID, "light"), 1, GL_FALSE, glm::value_ptr(camera.cascade_proj[i]));
        glClear(GL_DEPTH_BUFFER_BIT);

        // render geometry to the current cascade.
        player.draw(shader[SHADER_SHADOWMAP], shader[SHADER_SHADOWMAP], camera, false);
        level.draw(shader[SHADER_SHADOWMAP], shader[SHADER_SKYBOX], shader[SHADER_SHADOWMAP], shader[SHADER_LINE], camera);
    }

    // draw to screen texture.
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glPolygonOffset(0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, screen.screen_FBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // draw scene to post-process framebuffer.
    player.draw(shader[SHADER_CEL], shader[SHADER_LINE], camera, debug);
    level.draw(shader[SHADER_DEFAULT], shader[SHADER_SKYBOX], shader[SHADER_CEL], shader[SHADER_LINE], camera);
    level.skybox.draw(player.position, shader[SHADER_SKYBOX], camera);
    
    // finally, draw the screen framebuffer.
    screen.draw(shader[SHADER_FRAMEBUFFER], shader[SHADER_BLUR]);
}

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
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);         // gets rid of seams in skybox edges.
    glDepthFunc(GL_LEQUAL);                         // GL_LESS passes if new depth value is LESS than the stored depth value.
    glCullFace(GL_BACK);                            // cull back faces.
    glCullFace(GL_CCW);                             // counter clockwise indice order.
    
    std::array<Shader, SHADER_COUNT> shader = {                     // array of all shaders, 0 is always screen/post-process shader.
        Shader(GL_FILL, "framebuffer.vert", "framebuffer.frag"),    // post process framebuffer shader.
        Shader(GL_FILL, "shadow_map.vert",  "shadow_map.frag"),     // shadowmap shader.
        Shader(GL_FILL, "default.vert",     "default.frag"),        // default material shader.
        Shader(GL_FILL, "cel.vert",         "cel.frag"),            // character model shader.
        Shader(GL_LINE, "default.vert",     "line.frag"),           // wireframe shader.
        Shader(GL_FILL, "skybox.vert",      "skybox.frag"),         // skybox shader.
        Shader(GL_FILL, "blur.vert",        "blur.frag")            // blur shader.
    };

    // could prob organise this a bit better? tho i guess having some kind of 'game' class to create all of these is just redundant fluff.
    // i guess eventually would need some kind of save thing? idk if i rlly want to deal with that kind of thing though.
    Player player("boar_pig.gltf");
    Camera camera;
    Level level(player.current_level);  // load initial level based on player's level.
    ScreenTexture screen;               // should this be in camera?

    // fixed timestep setup. -- could get moved to a struct/something maybe.
    const double dt     = 1.0 / 60.0;   // base 60fps.
    // double t            = 0.0;          // time game has been running.
    double accumulator  = 0.0;          // only update game when > delta time.
    double global_speed = 1.0;          // controls global speed of the game.
    auto prev_time      = std::chrono::high_resolution_clock::now();

    // main loop.
    while(!glfwWindowShouldClose(window))
    {
        auto current_time       = std::chrono::high_resolution_clock::now();
        double frame_duration   = global_speed * (std::chrono::duration<double>(current_time - prev_time).count());
        // std::cout << "Frame duration: " << frame_duration << "ms" << "\n";
        // std::cout << "FPS: " << 1.0 / frame_duration << "\n";
        // std::cout << "Time: " << t << "ms" << "\n";

        accumulator += frame_duration;
        prev_time   = current_time;

        // update.
        for (; accumulator >= dt; accumulator -= dt)
        {
            update(player, camera, level, dt);
            // t += dt;
        }
        
        // draw.
        draw(camera, screen, shader, player, level);    // always once per frame.
		glfwSwapBuffers(window);                        // swap the back buffer with the front buffer.
        glfwPollEvents();                               // poll IO events.
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