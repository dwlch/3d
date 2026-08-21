#pragma once

#include "glad.h"
#include "GLFW/glfw3.h"

extern float HORIZONTAL;
extern float VERTICAL;

// camera.
extern double MOUSE_X;
extern double MOUSE_Y;
extern double MOUSE_X_PREV;
extern double MOUSE_Y_PREV;
extern double CAMERA_ZOOM;
extern bool CAMERA_INPUT;

// generic keys/buttons etc
extern int INPUT_0;
extern int INPUT_1;
extern int INPUT_2;
extern int INPUT_3;

extern int SPACE_PRESSED;

extern int INPUT_0_PREV;
extern int INPUT_1_PREV;
extern int INPUT_2_PREV;
extern int INPUT_3_PREV;

extern int SPACE_PRESSED_PREV;

// key callback.
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
void mouse_callback(GLFWwindow* window, int button, int action, int mods);
void cursor_callback(GLFWwindow *window, double x_position, double y_position);
void scroll_callback(GLFWwindow* window, double x_offset, double y_offset);
void update_inputs();