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

int INPUT_0       = 0;
int INPUT_1       = 0;
int INPUT_2       = 0;
int INPUT_3       = 0;

int SPACE_PRESSED = 0;

int INPUT_0_PREV = 0;
int INPUT_1_PREV = 0;
int INPUT_2_PREV = 0;
int INPUT_3_PREV = 0;

int SPACE_PRESSED_PREV = 0;

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
                    // if (INPUT_0_PREV != 1)
                    // {
                        INPUT_0 = 1;
                    // }
                    break;
                case GLFW_KEY_E:
                    // if (INPUT_1_PREV != 1)
                    // {
                        INPUT_1 = 1;
                    // }
                    break;

                // key/button inputs.
                case GLFW_KEY_F:
                    if (INPUT_2_PREV != 1)
                    {
                        INPUT_2 = 1;
                    }
                    break;
                case GLFW_KEY_R:
                    if (INPUT_3_PREV != 1)
                    {
                        INPUT_3 = 1;
                    }
                    break;

                // key/button inputs.
                case GLFW_KEY_SPACE:
                    // if (SPACE_PRESSED_PREV != 1)
                    // {
                    //     SPACE_PRESSED = 1;
                    // }
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
                case GLFW_KEY_Q:
                    INPUT_0 = 1;
                    break;
                case GLFW_KEY_E:
                    INPUT_1 = 1;
                    break;
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

void update_inputs()
{
    INPUT_0_PREV        = INPUT_0;
    INPUT_1_PREV        = INPUT_1;
    INPUT_2_PREV        = INPUT_2;
    INPUT_3_PREV        = INPUT_3;
    SPACE_PRESSED_PREV  = SPACE_PRESSED;

    // INPUT_0             = 0;
    // INPUT_1             = 0;
    INPUT_2             = 0;
    INPUT_3             = 0;
    SPACE_PRESSED       = 0;
}