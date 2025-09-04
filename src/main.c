#include "dbg.h"
#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CIMGUI_USE_GLFW
#define CIMGUI_USE_OPENGL3
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>
#include <cimgui_impl.h>

#include "cam.h"
#include "gfx.h"
#include "world.h"

#define DEFAULT_WINDOW_FACTOR_HEIGHT 0.8f
#define DEFAULT_ASPECT_RATIO (4.0f / 3.0f)

static GLFWwindow *window;
static int window_width, window_height;
static bool draw_wireframe = false;
static bool draw_lines = true;
static bool vsync = true;

static void glfw_error_cb(int error, const char *description);
static void glfw_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
static void GLAPIENTRY gl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam);

int main() {

    printf("Hello, from Kubik!\n");
#if KU_DEBUG
    printf("Debug build\n");
#endif

    glfwSetErrorCallback(glfw_error_cb);
    if (!glfwInit())
        return EXIT_FAILURE;

    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    window_height = mode->height * DEFAULT_WINDOW_FACTOR_HEIGHT;
    window_width = window_height * DEFAULT_ASPECT_RATIO;

#if KU_DEBUG
    const char *title = "kubik [debug]";
#else
    const char *title = "kubik";
#endif

    window = glfwCreateWindow(window_width, window_height, title, NULL, NULL);
    if (!window) {
        fprintf(stderr, "failed to create a glfw window");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(vsync ? 1 : 0);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwSetWindowSizeLimits(window, window_width, window_height, window_width, window_height);
    glfwSetKeyCallback(window, glfw_key_callback);

    if (!gladLoadGL(glfwGetProcAddress)) {
        fprintf(stderr, "failed to initialize glad");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(gl_debug_callback, NULL);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);

    printf("gl renderer: %s\n", glGetString(GL_RENDERER));
    printf("gl version:  %s\n", glGetString(GL_VERSION));

    if (!window) {
        fprintf(stderr, "Failed to initialize GLFW window\n");
        return EXIT_FAILURE;
    }

    // setup imgui
    igCreateContext(NULL);
    ImGuiIO *ioptr = igGetIO();
    // ioptr->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ioptr->IniFilename = NULL;
    igStyleCustomRound();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    bool showDemoWindow = true;
    bool showAnotherWindow = false;
    ImVec4 clearColor = {0.4f, 0.3f, 0.5f, 1.0f}; // Dark blueish background color

    double start_time = glfwGetTime();
    double prev_frame = glfwGetTime();

    gfx_initialize();

    struct cam camera;
    cam_initialize(&camera, window_width, window_height, to_rad(45.0f));

    struct world world;
    world_init(&world, 0);
    world_generate_initiale(&world);

    GLuint chunk_program = gfx_load_program("chunk", asset_path("chunk.vs"), asset_path("chunk.fs"));

    const char *atlas_paths[] = {
        asset_path("blocks/base.png"),       // 0
        asset_path("blocks/stone.png"),      // 1
        asset_path("blocks/dirt.png"),       // 2
        asset_path("blocks/grass_top.png"),  // 3
        asset_path("blocks/grass_side.png"), // 4
    };

    int atlas_depth = 0;
    GLuint chunk_atlas = gfx_load_atlas("atlas", atlas_paths, 5, 8, 8, 5, &atlas_depth);

    vec3 ro, rd;
    struct world_hit hit = {0};

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double curr_frame = glfwGetTime();
        const double dt = curr_frame - prev_frame;
        prev_frame = curr_frame;

        cam_fps_update(&camera, window, dt);

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            // cursor screen pos
            double mouse_x, mouse_y;
            glfwGetCursorPos(window, &mouse_x, &mouse_y);
            cam_screen_to_ray(&camera, (int)mouse_x, (int)mouse_y, &ro, &rd);
            world_trace(&world, ro, rd, 100.0f, &hit);
        }

        // rendering start
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        igNewFrame();

        gfx_enable_lines(draw_lines);
        gfx_start_frame(&camera);

        vec3 hit_pos = v3_add(v3_cast(hit.pos), (vec3){0.5f, 0.5f, 0.5f});
        dbg_cube(hit_pos, (vec3){0.54f, 0.54f, 0.54f}, C32_WHITE);
        dbg_line(hit_pos, v3_add(hit_pos, v3_cast(hit.normal)), C32_YELLOW);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
#if 1
        glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#else
        glClear(GL_DEPTH_BUFFER_BIT);
#endif

        glViewport(0, 0, window_width, window_height);
        glPolygonMode(GL_FRONT_AND_BACK, draw_wireframe ? GL_LINE : GL_FILL);

        // tmp_world_draw(&world, &camera, chunk_program, chunk_atlas, atlas_depth);
        world_draw(&world, &camera, chunk_program, chunk_atlas, atlas_depth);

        igSetNextWindowPos((ImVec2){0, 0}, ImGuiCond_Always, (ImVec2){0, 0});
        igBegin(" ", NULL, 0);
        {
            igText("application average %.3f ms/frame (%.1f FPS)", 1000.0f / igGetIO()->Framerate, igGetIO()->Framerate);
            igText("time since start: %.1f s", glfwGetTime() - start_time);
            igText("wireframe: %s (F1 to toggle)", draw_wireframe ? "[ON]" : "[OFF]");
            igText("vsync: %s (F2 to toggle)", vsync ? "[ON]" : "[OFF]");
            igText("debug lines: %s (F3 to toggle)", draw_lines ? "[ON]" : "[OFF]");

            igSeparatorText("Player");
            igText("position: (%d, %d, %d)", (int)camera.pos.x, (int)camera.pos.y, (int)camera.pos.z);
            igText("chunk: (%d, %d, %d)", u_idiv((int)camera.pos.x, C_SIZE), u_idiv((int)camera.pos.y, C_SIZE), u_idiv((int)camera.pos.z, C_SIZE));
            igText("ro: %.1f, %.1f, %.1f", ro.x, ro.y, ro.z);
            igText("rd: %.1f, %.1f, %.1f", rd.x, rd.y, rd.z);
            igText("hit: %d", hit.did_hit);
            if (hit.did_hit) {
                igText("hit block: %s [%i]", block_name(hit.block), (int)hit.block);
                igText("hit pos: (%d, %d, %d)", hit.pos.x, hit.pos.y, hit.pos.z);
                igText("hit normal: (%d, %d, %d)", hit.normal.x, hit.normal.y, hit.normal.z);
                igText("hit distance: %.1f", hit.distance);
            }

            igSeparatorText("World");
            igText("Seed: %d", world.seed);
            igText("Chunk Count: %d", world.chunk_count);
            igText("Hunk Count: %d", world.hunk_count);
            igText("Draw Calls: %d", world.rnd.draw_count);
            igText("Draw Commands: %d", world.rnd.added_cmds_count);

            for (int i = 0; i < world.rnd.draw_count; i++) {
                struct drawbuf *buf = world.rnd.drawbuf[i];
                if (igTreeNode_StrStr("drawbuffer", "buffer %d", i)) {
                    igText("  Buffer size: %zu bytes [%zu MB]", BUFFER_SIZE, BUFFER_SIZE / (1024 * 1024));
                    igText("  Page size: %zu bytes [%zu KB]", PAGE_SIZE, PAGE_SIZE / 1024);
                    igText("  Total pages: %zu", PAGE_COUNT);
                    igText("  Free pages: %zu", buf->free_pages);
                    igText("  Used pages: %zu", PAGE_COUNT - buf->free_pages);
                    igText("  Utilization: %.1f%%", 100.0 * (PAGE_COUNT - buf->free_pages) / PAGE_COUNT);
                    igText("  Last Alloc: %d", buf->last_alloc_word);
                    igTreePop();
                }
            }
        }
        igEnd();

        gfx_end_frame();
        igRender();
        ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());
        glfwSwapBuffers(window);
    }

    gfx_shutdown();
    glfwTerminate();
    return EXIT_SUCCESS;
}

static void glfw_error_cb(int error, const char *description) {
    fprintf(stderr, "[GLFW] %i: %s", error, description);
}

static void glfw_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    (void)(scancode);
    (void)(action);
    (void)(mods);

    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        return;
    }

    if (action == GLFW_PRESS && key == GLFW_KEY_F1) {
        draw_wireframe = !draw_wireframe;
    }

    if (action == GLFW_PRESS && key == GLFW_KEY_F2) {
        vsync = !vsync;
        glfwSwapInterval(vsync ? 1 : 0);
    }

    if (action == GLFW_PRESS && key == GLFW_KEY_F3) {
        draw_lines = !draw_lines;
    }

    // if (igGetIO()->WantCaptureKeyboard)return;
}

// OpenGL debug callback function
static void GLAPIENTRY gl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam) {
    // Skip notification messages for cleaner output (optional)
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
        return;
    }

    const char *severity_str = gl_severity_str(severity);
    const char *type_str = gl_type_str(type);
    const char *source_str = gl_source_str(source);

    fprintf(stderr, "[OpenGL %s] %s/%s (ID: %u): %s\n", severity_str, source_str, type_str, id, message);

// Break on high severity errors in debug builds
#ifndef NDEBUG
    if (severity == GL_DEBUG_SEVERITY_HIGH && type == GL_DEBUG_TYPE_ERROR) {
        fprintf(stderr, "Breaking due to high severity OpenGL error!\n");
#    ifdef _WIN32
        __debugbreak();
#    else
        __builtin_trap();
#    endif
    }
#endif
}
