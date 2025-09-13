#ifndef DBG_H
#define DBG_H
#include <glad/gl.h>
#include <string.h>

static int gl_get_gpu_memory_info(int *total_mem, int *avail_mem, float *ratio) {
    const char *vendor = (const char *)glGetString(GL_VENDOR);
    if (!vendor)
        return 0;

    // https://registry.khronos.org/OpenGL/extensions/NVX/NVX_gpu_memory_info.txt
    if (strstr(vendor, "NVIDIA") != NULL) {
        glGetIntegerv(0x9047, total_mem);
        glGetIntegerv(0x9049, avail_mem);
        *ratio = 1.0f - ((float)(*avail_mem) / (float)(*total_mem));
        return 1;
    }

    return 0; // anything besides NVIDIA I guess
}

static const char *gl_severity_str(GLenum severity) {
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        return "HIGH";
    case GL_DEBUG_SEVERITY_MEDIUM:
        return "MEDIUM";
    case GL_DEBUG_SEVERITY_LOW:
        return "LOW";
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        return "NOTIFICATION";
    default:
        return "UNKNOWN";
    }
}

static const char *gl_type_str(GLenum type) {
    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        return "ERROR";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        return "DEPRECATED_BEHAVIOR";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        return "UNDEFINED_BEHAVIOR";
    case GL_DEBUG_TYPE_PORTABILITY:
        return "PORTABILITY";
    case GL_DEBUG_TYPE_PERFORMANCE:
        return "PERFORMANCE";
    case GL_DEBUG_TYPE_MARKER:
        return "MARKER";
    case GL_DEBUG_TYPE_PUSH_GROUP:
        return "PUSH_GROUP";
    case GL_DEBUG_TYPE_POP_GROUP:
        return "POP_GROUP";
    case GL_DEBUG_TYPE_OTHER:
        return "OTHER";
    default:
        return "UNKNOWN";
    }
}

static const char *gl_source_str(GLenum source) {
    switch (source) {
    case GL_DEBUG_SOURCE_API:
        return "API";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        return "WINDOW_SYSTEM";
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        return "SHADER_COMPILER";
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        return "THIRD_PARTY";
    case GL_DEBUG_SOURCE_APPLICATION:
        return "APPLICATION";
    case GL_DEBUG_SOURCE_OTHER:
        return "OTHER";
    default:
        return "UNKNOWN";
    }
}

#endif // DBG_H