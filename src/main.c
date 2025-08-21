#include "dbg.h"
#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void glfw_error_cb(int error, const char *description);
static void glfw_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
static void GLAPIENTRY gl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam);

struct chunk_list {
    struct chunk *chunks;
    size_t count;
    size_t capacity;
};

void init_chunk_list(struct chunk_list *list, int capacity) {
    list->capacity = capacity;
    list->chunks = malloc(sizeof(struct chunk) * capacity);
    list->count = 0;
}

void add_chunk_list(struct chunk_list *list, struct chunk *s) {
    if (list->count < list->capacity) {
        list->chunks[list->count++] = *s;
    }
}

struct chunk *find_chunk(struct chunk_list *list, int x, int y, int z) {
    for (size_t i = 0; i < list->count; i++) {
        struct chunk *c = &list->chunks[i];
        if (c->x == x && c->y == y && c->z == z) {
            return c;
        }
    }
    return NULL;
}

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
    glfwSwapInterval(0);
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
    world_init_demo(&world);

    GLuint chunk_program = gfx_load_program("chunk", asset_path("chunk.vs"), asset_path("chunk.fs"));

    const char *atlas_paths[] = {
        asset_path("stone.png"),
    };

    int atlas_depth = 0;
    GLuint chunk_atlas = gfx_load_atlas("atlas", atlas_paths, 1, 8, 8, 8, &atlas_depth);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double curr_frame = glfwGetTime();
        const double dt = curr_frame - prev_frame;
        prev_frame = curr_frame;

        cam_fps_update(&camera, window, dt);

        // rendering start
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        igNewFrame();

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
        tmp_world_draw(&world, cam_view_proj(&camera), chunk_program, chunk_atlas, atlas_depth);
        gfx_tmp_draw_lines(&camera);

        igSetNextWindowPos((ImVec2){0, 0}, ImGuiCond_Always, (ImVec2){0, 0});
        igBegin(" ", NULL, 0);
        {
            igText("application average %.3f ms/frame (%.1f FPS)", 1000.0f / igGetIO()->Framerate, igGetIO()->Framerate);
            igText("camera position: (%.1f, %.1f, %.1f)", camera.pos.x, camera.pos.y, camera.pos.z);
            vec3 cam_rot_euler = quat_to_euler(camera.rot);
            igText("camera rotation: (%.1f, %.1f, %.1f)", cam_rot_euler.x, cam_rot_euler.y, cam_rot_euler.z);
        }
        igEnd();

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
