# Produce runnable native and browser builds

## Design

CMake pins and verifies SDL3, Lua and cJSON downloads. A common shader pair is loaded from the assets directory with a platform-appropriate GLSL version. Tests run the engine independently of the window system.
