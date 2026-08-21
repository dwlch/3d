#pragma once

#define WINDOW_WIDTH        1200    // global window width.
#define WINDOW_HEIGHT       960    // global window height.
#define SHADER_COUNT        8       // total levels.
#define NUM_CASCADES        3       // number of shadowmap cascades.
#define SHADOWMAP_SIZE      4096    // resolution of the shadowmap texture. 2048. 4096.
#define SAMPLE_RATE         48000

#define FONT_MAIN           "FragmentMono-Regular.png"

// shaders.
#define SHADER_FRAMEBUFFER  0
#define SHADER_SHADOWMAP    1
#define SHADER_DEFAULT      2
#define SHADER_CEL          3
#define SHADER_LINE         4
#define SHADER_SKYBOX       5
#define SHADER_BLUR         6
#define SHADER_TEXT         7

