#pragma once

#include "glad.h"
#include "GLFW/glfw3.h"

// movement axis/dpad/wasd/left stick etc.
extern float AXIS_0_UP;
extern float AXIS_0_DOWN;
extern float AXIS_0_LEFT;
extern float AXIS_0_RIGHT;

// camera axis/right stick etc.
extern float AXIS_1_UP;
extern float AXIS_1_DOWN;
extern float AXIS_1_LEFT;
extern float AXIS_1_RIGHT;

// generic keys/buttons etc
extern int INPUT_0;
extern int INPUT_1;

extern int INPUT_2;
extern int INPUT_3;

extern int SPACE_PRESSED;

// key callback.
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);