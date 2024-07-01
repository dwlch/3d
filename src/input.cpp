#include "input.hpp"
#include <glm/glm.hpp>
#include <iostream>

float AXIS_0_UP     = 0.0f;
float AXIS_0_DOWN   = 0.0f;
float AXIS_0_LEFT   = 0.0f;
float AXIS_0_RIGHT  = 0.0f;

float AXIS_1_UP     = 0.0f;
float AXIS_1_DOWN   = 0.0f;
float AXIS_1_LEFT   = 0.0f;
float AXIS_1_RIGHT  = 0.0f;

int INPUT_0       = 0.0f;
int INPUT_1       = 0.0f;

int INPUT_2       = 0.0f;
int INPUT_3       = 0.0f;

int SPACE_PRESSED = 0.0f;

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    switch (action)
    {
        case GLFW_PRESS:
            switch (key)
            {
                // exit game when escape key is pressed.
                case GLFW_KEY_ESCAPE:
                    glfwSetWindowShouldClose(window, 1);
                    break;

                // key/button inputs.
                case GLFW_KEY_Q:
                    INPUT_0 = 1;
                    break;
                case GLFW_KEY_E:
                    INPUT_1 = 1;
                    break;

                // key/button inputs.
                case GLFW_KEY_F:
                    INPUT_2 = 1;
                    break;
                case GLFW_KEY_R:
                    INPUT_3 = 1;
                    break;

                // key/button inputs.
                case GLFW_KEY_SPACE:
                    SPACE_PRESSED = 1;
                    break;

                // axis 0.
                case GLFW_KEY_W:
                    AXIS_0_UP = 1.0f;
                    break;
                case GLFW_KEY_A:
                    AXIS_0_LEFT = 1.0f;
                    break;
                case GLFW_KEY_S:
                    AXIS_0_DOWN = 1.0f;
                    break;
                case GLFW_KEY_D:
                    AXIS_0_RIGHT = 1.0f;
                    break;

                // axis 1.
                case GLFW_KEY_UP:
                    AXIS_1_UP = 1.0f;
                    break;
                case GLFW_KEY_LEFT:
                    AXIS_1_LEFT = 1.0f;
                    break;
                case GLFW_KEY_DOWN:
                    AXIS_1_DOWN = 1.0f;
                    break;
                case GLFW_KEY_RIGHT:
                    AXIS_1_RIGHT = 1.0f;
                    break;
                default:
                    break;
            }
            break;
        case GLFW_RELEASE:
            switch (key)
            {
                case GLFW_KEY_Q:
                    INPUT_0 = 0;
                    break;
                case GLFW_KEY_E:
                    INPUT_1 = 0;
                    break;

                // key/button inputs.
                case GLFW_KEY_F:
                    INPUT_2 = 0;
                    break;
                case GLFW_KEY_R:
                    INPUT_3 = 0;
                    break;

                // key/button inputs.
                case GLFW_KEY_SPACE:
                    SPACE_PRESSED = 0;
                    break;

                // axis 0.
                case GLFW_KEY_W:
                    AXIS_0_UP = 0.0f;
                    break;
                case GLFW_KEY_A:
                    AXIS_0_LEFT = 0.0f;
                    break;
                case GLFW_KEY_S:
                    AXIS_0_DOWN = 0.0f;
                    break;
                case GLFW_KEY_D:
                    AXIS_0_RIGHT = 0.0f;
                    break;

                // axis 1.
                case GLFW_KEY_UP:
                    AXIS_1_UP = 0.0f;
                    break;
                case GLFW_KEY_LEFT:
                    AXIS_1_LEFT = 0.0f;
                    break;
                case GLFW_KEY_DOWN:
                    AXIS_1_DOWN = 0.0f;
                    break;
                case GLFW_KEY_RIGHT:
                    AXIS_1_RIGHT = 0.0f;
                    break;
                default:
                    break;
            }
            break;
        case GLFW_REPEAT:
            switch (key)
            {
                // key/button inputs.
                case GLFW_KEY_SPACE:
                    SPACE_PRESSED = 0;
                    break;
                // key/button inputs.
                case GLFW_KEY_F:
                    INPUT_2 = 0;
                    break;
                case GLFW_KEY_R:
                    INPUT_3 = 0;
                    break;
                default:
                    break;
            }
            break;
        
        default:
            break;
    }
}