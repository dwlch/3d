#include "input.hpp"
#include "defines.hpp"
#include <glm/glm.hpp>
#include <iostream>

// movement input.
float HORIZONTAL;
float VERTICAL;

// camera/mouse input.
double MOUSE_X;
double MOUSE_Y;
double MOUSE_X_PREV;
double MOUSE_Y_PREV;
double CAMERA_ZOOM;
bool CAMERA_INPUT       = false;
float mouse_smoothing   = 0.001f;

// spacebar input.
int SPACE_PRESSED = 0;
int SPACE_PRESSED_PREV = 0;

// extra buttons.
int INPUT_0         = 0;
int INPUT_1         = 0;
int INPUT_2         = 0;
int INPUT_3         = 0;
int INPUT_0_PREV    = 0;
int INPUT_1_PREV    = 0;
int INPUT_2_PREV    = 0;
int INPUT_3_PREV    = 0;



void scroll_callback(GLFWwindow* window, double x_offset, double y_offset)
{
    CAMERA_ZOOM = y_offset;
}

void cursor_callback(GLFWwindow *window, double x_position, double y_position)
{
    // only move the camera if holding down left/right click.
    if (CAMERA_INPUT)
    {
        MOUSE_X         += (MOUSE_X_PREV - x_position) * mouse_smoothing;
        MOUSE_Y         += (MOUSE_Y_PREV - y_position) * mouse_smoothing;
        MOUSE_X_PREV    = x_position;
        MOUSE_Y_PREV    = y_position;
    }
}

void mouse_callback(GLFWwindow* window, int button, int action, int mods)
{
    switch (button)
    {
        case GLFW_MOUSE_BUTTON_LEFT:
            switch (action)
            {
                case GLFW_PRESS:
                    CAMERA_INPUT = true;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    glfwGetCursorPos(window, &MOUSE_X_PREV, &MOUSE_Y_PREV);
                    break;
                case GLFW_RELEASE:
                    CAMERA_INPUT = false;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    break;
                default:
                    break;
            }
        break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            switch (action)
            {
                case GLFW_PRESS:
                    CAMERA_INPUT = true;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    glfwGetCursorPos(window, &MOUSE_X_PREV, &MOUSE_Y_PREV);
                    break;
                case GLFW_RELEASE:
                    CAMERA_INPUT = false;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    break;
                default:
                    break;
            }
        break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            switch (action)
            {
                case GLFW_PRESS:
                    
                    break;
                case GLFW_RELEASE:

                    break;
                case GLFW_REPEAT:
                
                    break;
                default:
                    break;
            }
        break;
        default:
            break;
    }
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    switch (key)
    {
        case GLFW_KEY_ESCAPE:
            switch (action)
            {
            case GLFW_PRESS:
                glfwSetWindowShouldClose(window, 1);
                break;
            case GLFW_RELEASE:
                
                break;
            case GLFW_REPEAT:
                
                break;
            default:
                break;
            }
        break;

        // WASD / DPAD CONTROLS.
        case GLFW_KEY_W:
            switch (action)
            {
            case GLFW_PRESS:
                VERTICAL += 1.0f;
                break;
            case GLFW_RELEASE:
                VERTICAL -= 1.0f;
                break;
            case GLFW_REPEAT:
                
                break;
            default:
                break;
            }
        break;
        case GLFW_KEY_S:
            switch (action)
            {
            case GLFW_PRESS:
                VERTICAL -= 1.0f;
                break;
            case GLFW_RELEASE:
                VERTICAL += 1.0f;
                break;
            case GLFW_REPEAT:
                
                break;
            default:
                break;
            }
        break;
        case GLFW_KEY_A:
            switch (action)
            {
            case GLFW_PRESS:
                HORIZONTAL += 1.0f;
                break;
            case GLFW_RELEASE:
                HORIZONTAL -= 1.0f;
                break;
            case GLFW_REPEAT:
                
                break;
            default:
                break;
            }
        break;
        case GLFW_KEY_D:
            switch (action)
            {
            case GLFW_PRESS:
                HORIZONTAL -= 1.0f;
                break;
            case GLFW_RELEASE:
                HORIZONTAL += 1.0f;
                break;
            case GLFW_REPEAT:
                
                break;
            default:
                break;
            }
        break;

        // spacebar
        case GLFW_KEY_SPACE:
            switch (action)
            {
            case GLFW_PRESS:
                SPACE_PRESSED = 1;
                break;
            case GLFW_RELEASE:
                SPACE_PRESSED = 0;
                break;
            case GLFW_REPEAT:
                SPACE_PRESSED = 0;
                break;
            default:
                break;
            }
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
    CAMERA_ZOOM         = 0.0f;

    MOUSE_X = 0.0;
    MOUSE_Y = 0.0;

    INPUT_0             = 0;
    INPUT_1             = 0;
    INPUT_2             = 0;
    INPUT_3             = 0;
    SPACE_PRESSED       = 0;
}