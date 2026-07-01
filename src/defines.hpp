#pragma once

#define WINDOW_WIDTH        1100    // global window width.
#define WINDOW_HEIGHT       900    // global window height.
#define SHADER_COUNT        7       // total levels.
#define NUM_CASCADES        3       // number of shadowmap cascades.
#define SHADOWMAP_SIZE      4096    // resolution of the shadowmap texture. 2048. 4096.

// shaders.
#define SHADER_FRAMEBUFFER  0
#define SHADER_SHADOWMAP    1
#define SHADER_DEFAULT      2
#define SHADER_CEL          3
#define SHADER_LINE         4
#define SHADER_SKYBOX       5
#define SHADER_BLUR         6

