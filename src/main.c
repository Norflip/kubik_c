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

#define DEFAULT_WINDOW_FACTOR_HEIGHT 0.8f
#define DEFAULT_ASPECT_RATIO (4.0f / 3.0f)

static GLFWwindow *window;
static int window_width, window_height;
static bool draw_wireframe = false;

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

#define DRAW_DEMO_TRIS 0

#if DRAW_DEMO_TRIS
    // Setting up the triangle data
    // ----------------------------

    float vertices[] = {-1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    GLuint attribPos = 0;
    GLuint attribCol = 1;

    /**************** VBO ***************/
    unsigned int hctVBO; // hello colorful triangle vbo
    // glGenBuffers(1, &VBO); // Way to go before openGl 4.5
    // glBindBuffer(GL_ARRAY_BUFFER, VBO); // Binding to openGl context was necessary
    //   replaced with:
    glCreateBuffers(1, &hctVBO); // uses DSA. This is the way.

    // glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    //   replaced with:
    glNamedBufferStorage(hctVBO, sizeof(vertices), vertices, GL_DYNAMIC_STORAGE_BIT);
    //                    ^^^  needed, since there is no context binding.

    /**************** VAO ***************/
    unsigned int hctVAO;
    // glGenVertexArrays(1, &VAO);
    // glBindVertexArray(VAO);
    //   replaced with:
    glCreateVertexArrays(1, &hctVAO); // This is the way.

    // As there is, by definition, no context binding in DSA, then we need to
    //"bind" vao with vbo explicitely, like linking 2 indexes in a database.

    GLuint vaoBindingPoint = 0;                   // A binding point in VAO. See GL_MAX_VERTEX_ATTRIB_BINDINGS
    glVertexArrayVertexBuffer(hctVAO,             // vao to bind
                              vaoBindingPoint,    // Could be 1, 2... if there were several vbo to source.
                              hctVBO,             // VBO to bound at "vaoBindingPoint".
                              0,                  // offset of the first element in the buffer hctVBO.
                              6 * sizeof(float)); // stride == 3 position floats + 3 color floats.

    // glEnableVertexAttribArray(attribPos);
    // glEnableVertexAttribArray(attribCol);
    //   replaced with:
    glEnableVertexArrayAttrib(hctVAO, attribPos); // Need to precise vao, as there is no context binding in DSA style
    glEnableVertexArrayAttrib(hctVAO, attribCol); // Meaning no current vao is bound to the opengl context.

    // glVertexAttribPointer(attribPos, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    // glVertexAttribPointer(attribCol, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)( 3*sizeof(float) ));
    //   replaced with:
    glVertexArrayAttribFormat(hctVAO, attribPos, 3, GL_FLOAT, false, 0); // Need to precise vao, as there is no context binding in DSA
    glVertexArrayAttribFormat(hctVAO, attribCol, 3, GL_FLOAT, false,
                              3 * sizeof(float)); // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glVertexAttribFormat.xhtml

    // Explicit binding of an attribute to a vao binding point
    glVertexArrayAttribBinding(hctVAO, attribPos, vaoBindingPoint);
    glVertexArrayAttribBinding(hctVAO, attribCol, vaoBindingPoint);

    GLuint test_program = gfx_load_program("test", asset_path("hello.vs"), asset_path("hello.fs"));
#endif

    gfx_initialize();

    struct cam camera;
    cam_initialize(&camera, window_width, window_height, to_rad(45.0f));

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

        // glEnable(GL_DEPTH_TEST);
        // glEnable(GL_CULL_FACE);
        // glCullFace(GL_BACK);
#if 1
        glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#else
        glClear(GL_DEPTH_BUFFER_BIT);
#endif

        glViewport(0, 0, window_width, window_height);
        glPolygonMode(GL_FRONT_AND_BACK, draw_wireframe ? GL_LINE : GL_FILL);
        {
            dbg_line((vec3){0, 0, 0}, (vec3){1, 1, 1}, C32_BLUE);
            dbg_aabb((vec3){0, 0, 0}, (vec3){16, 16, 16}, C32_CYAN);

#if DRAW_DEMO_TRIS
            glUseProgram(test_program);
            float *view_proj = cam_view_proj_ptr(&camera);
            glUniformMatrix4fv(glGetUniformLocation(test_program, "vp"), 1, GL_FALSE, view_proj);
            glBindVertexArray(hctVAO);
            glDrawArrays(GL_TRIANGLES, 0, 3);
#endif
        }

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
