/*
opengl simple game engine.
version start date : 22 08 2022.
*/

// c++ includes.
#include <iostream>     // console printing.
#include <array>        // shader array.
#include <chrono>       // needed for timestep.
#include <thread>       // multithreading.
#include <memory>

// audio related
// audio stuff
#include <mfapi.h>

// GL includes.
#include "glad.h"
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

// internal includes.
#include "audio.hpp"
#include "collision.hpp"    // gjk + epa collision implementation, also level loading.
#include "defines.hpp"      // global variables.
#include "camera.hpp"       // camera + shadowmap.
#include "shader.hpp"       // shader loading.
#include "draw.hpp"         // gltf model loading + some primitive mesh stuff.

#include "gltf.hpp"
#include "player.hpp"       // player controller.
#include "npc.hpp"          // npcs. (might factor some of this elsewhere).
#include "level.hpp"        // handles level loading.
#include "input.hpp"        // input handler (needs some work).

// global stuff.
// plan to use this enum to toggle level editor.
enum Mode
{
    GAME,
    EDIT
};

Mode mode       = Mode::GAME;
bool debug      = true;
bool finished   = false;

// basically this just stores everything that is loaded while the game is running.
struct Game
{
    std::vector<std::unique_ptr<Collider>> colliders;   // vector array of colliders to test against player in update.
    std::vector<std::unique_ptr<Collider>> triggers;    // vector array of triggers in the level.
    std::vector<Npc> npcs;                              // npc array
    std::vector<Sound> sounds;
    Skybox skybox;
    Player player;

};

struct Time
{
    double dt;    
    double duration     = 0.0;          // time game has been running.
    double accumulator  = 0.0;          // only update game when > delta time.
    double global_speed = 1.0;          // controls global speed of the game.
    double FPS;
    std::chrono::time_point<std::chrono::high_resolution_clock> prev_time = std::chrono::high_resolution_clock::now();

    Time(int refresh_rate)
    {
        Time::dt = 1.0 / refresh_rate;
    };

    void get_time()
    {
        auto current_time       = std::chrono::high_resolution_clock::now();
        double frame_duration   = global_speed * (std::chrono::duration<double>(current_time - prev_time).count());
        FPS                     = 1.0 / frame_duration;

        // std::cout << "Frame duration: " << frame_duration << "ms" << "\n";
        // std::cout << "FPS: " << 1.0 / frame_duration << "\n";
        // std::cout << "Time: " << duration << "ms" << "\n";

        accumulator += frame_duration;
        prev_time   = current_time;
    };
};

void update(Player &player, Camera &camera, std::unique_ptr<Level> &level, Skybox &skybox, Textbox &textbox, Win32Audio &audio, Sound &bgm, Time &time)
{
    // Sleep(5 * 17);
    time.get_time();
    float delta_time = (float)time.dt;

    // update game logic at the rate specified by delta time (defaults to the monitors refresh rate).
    for (; time.accumulator >= time.dt; time.accumulator -= time.dt)
    {
        // game logic/collisions/movement etc.
        camera.get_input(player.auto_cam_yaw, delta_time);      // camera input, calculates camera orientation vec3.
        player.update(level, skybox, bgm, camera, delta_time);  // player input and movement, sent a vector of colliders.
        camera.update(player.camera_lookat);                    // update camera matrix using target position.

        int count = 0;
        for (size_t i = 0; i < level->npcs.size(); ++i)
        {
            float distance = glm::length(glm::vec3(player.position.x - level->npcs[i].position.x, player.position.y - level->npcs[i].position.y, player.position.z - level->npcs[i].position.z));
            if (distance < 5.0f)
            {
                level->npcs[i].target_animation = 1;
                textbox.content                 = level->npcs[i].dialogue;
                count++;
            }

            if (count == 0)
            {
                level->npcs[i].target_animation = 0;
                textbox.content                 = {};
            }
            level->npcs[i].update(delta_time);
        }


        // audio.
        Win32AudioWriteContext write_context(&audio, delta_time);



        bgm.play(write_context);
        player.sound.play(write_context);


        write_context.release(&audio);

        
        update_inputs();
        // time.duration += time.dt;
    }
}

void draw(Player &player, Camera &camera, std::unique_ptr<Level> &level, Skybox &skybox, Textbox &textbox, ScreenTexture screen, std::array<Shader, SHADER_COUNT> &shader)
{
    // draw to shadowmaps.
    glViewport(0, 0, SHADOWMAP_SIZE, SHADOWMAP_SIZE);
    glPolygonOffset(6.0f, 1.0f); // factor, unit. (adjusts shadow artifacting)
    glBindFramebuffer(GL_FRAMEBUFFER, camera.FBO);
    glUseProgram(shader[SHADER_SHADOWMAP].ID);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // get the shadowmap cascade projections according to light direction.
    camera.get_cascades(level->light);
    
    // do for each cascade in the shadowmap array.
    for (int i = 0; i < NUM_CASCADES; ++i)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, camera.depth_maps[i], 0);
        glUniformMatrix4fv(glGetUniformLocation(shader[SHADER_SHADOWMAP].ID, "light"), 1, GL_FALSE, glm::value_ptr(camera.cascade_proj[i]));
        glClear(GL_DEPTH_BUFFER_BIT);

        // render geometry to the current cascade.
        player.draw(shader[SHADER_SHADOWMAP].ID, shader[SHADER_SHADOWMAP].ID, camera, false);
        level->draw(glm::vec3(0.0f), glm::quat(glm::vec3(0.0f)), glm::vec3(1.0f), shader[SHADER_SHADOWMAP].ID, camera, glm::vec3(1.0f));
        for (size_t i = 0; i < level->npcs.size(); ++i)
        {
            level->npcs[i].draw(shader[SHADER_SHADOWMAP].ID, shader[SHADER_SHADOWMAP].ID, camera, false);
        }
    }

    // draw to screen texture.
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glPolygonOffset(0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, screen.screen_FBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // draw scene to post-process framebuffer.
    player.draw(shader[SHADER_CEL].ID, shader[SHADER_LINE].ID, camera, debug);
    level->draw(glm::vec3(0.0f), glm::quat(glm::vec3(0.0f)), glm::vec3(1.0f), shader[SHADER_DEFAULT].ID, camera, glm::vec3(1.0f));
    for (size_t i = 0; i < level->npcs.size(); ++i)
    {
        level->npcs[i].draw(shader[SHADER_CEL].ID, shader[SHADER_LINE].ID, camera, debug);
    }
    skybox.draw(player.position, shader[SHADER_SKYBOX].ID, camera);
    
    // finally, draw the screen framebuffer.
    screen.draw(shader[SHADER_FRAMEBUFFER].ID, shader[SHADER_BLUR].ID);

    // draw text/ui stuff after screenbuffer (so it doesn't get effected by the screenbuffer shader).
    textbox.draw(0.0f, 0.0f, shader[SHADER_TEXT].ID);
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
    glfwSetWindowPos(window, (mode->width / 2) - (WINDOW_WIDTH / 2), (mode->height / 2) - (WINDOW_HEIGHT / 2));
    
    // checks if window creation failed, terminates glfw if so.
    if (!window)
    {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);                             // makes the created window the 'OpenGL context'.
    glfwSwapInterval(1);                                        // enable vsync.
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);  // set mouse mode here (for now using normal mode until i re-add mouse input).
    if (glfwRawMouseMotionSupported())
    {
        std::cout << "glfwRawMouseMotionSupported\n";
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
    
    // set glfw input callbacks.
    glfwSetKeyCallback(window, key_callback);                   // set the key callback to input.hpp's key_callback function.
    glfwSetMouseButtonCallback(window, mouse_callback);         // mouse button inputs.
    glfwSetCursorPosCallback(window, cursor_callback);          // cursor position.
    glfwSetScrollCallback(window, scroll_callback);             // scroll wheel input.
    
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
        Shader(GL_FILL, "blur.vert",        "blur.frag"),           // blur shader.
        Shader(GL_FILL, "text.vert",        "text.frag")
    };

    // could prob organise this a bit better? tho i guess having some kind of 'game' class to create all of these is just redundant fluff.
    Win32Audio audio = {};
    Win32AudioStart(&audio, SAMPLE_RATE, 2, SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT);

    
    Time time(mode->refreshRate);
    Player player;
    Camera camera;
    ScreenTexture screen;
    Sound bgm;
    Skybox skybox;
    Textbox textbox;

    // level.
    std::unique_ptr<Level> level = std::make_unique<Level>();
    level->load_level(player.current_level, skybox, bgm);

    // random seed -- use system date/time prob.
    std::srand(std::time(0));

    // main loop.
    while (!glfwWindowShouldClose(window))
    {
        update( player, camera, level, skybox, textbox, audio, bgm, time);
        draw(   player, camera, level, skybox, textbox, screen, shader);
		glfwSwapBuffers(window);    // swap the back buffer with the front buffer.
        glfwPollEvents();           // poll IO events.
    }

	glfwDestroyWindow(window);      // destroy window prior to ending program.
	glfwTerminate();                // end GLFW.
	return 0;
}