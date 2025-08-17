cmake_minimum_required(VERSION 3.5.0)
project(kubik VERSION 0.1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
string(TOLOWER ${CMAKE_BUILD_TYPE} build_type)
set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${build_type}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")


set(LIB_DIR "${CMAKE_SOURCE_DIR}/libs")

#include_directories(deps)
#include(deps/deps.cmake)
#add_subdirectory(deps/tinycthread)

include_directories(${LIB_DIR})

# GLAD
add_subdirectory(${LIB_DIR}/glad)

# GLFW
option(GLFW_BUILD_DOCS OFF)
option(GLFW_BUILD_EXAMPLES OFF)
option(GLFW_BUILD_TESTS OFF)
option(GLFW_INSTALL OFF)
add_subdirectory(${LIB_DIR}/glfw)
set(GLFW_INCLUDE_DIR ${LIB_DIR}/glfw/include)

# CIMGUI
# probably a bad name and location, but it works for now
include(${LIB_DIR}/cimgui.cmake)

# Main executable
set(SRC_FILES
  src/main.c
  src/maf.c
  src/cam.c
  src/gfx.c
  src/chunk.c
)

add_executable(kubik ${SRC_FILES})
target_include_directories(kubik PUBLIC
    ${OPENGL_INCLUDE_DIR}
    ${GLFW_INCLUDE_DIR}
    ${GLAD_INCLUDE_DIR}
    ${CIMGUI_INCLUDE_DIRS}
    )

target_link_libraries(kubik ${OPENGL_LIBRARIES} glfw glad cimgui)
target_compile_definitions(kubik PUBLIC 
  -DKU_ASSET_PATH="${CMAKE_SOURCE_DIR}/assets/"
  -DGLFW_INCLUDE_NONE
  $<$<CONFIG:Debug>:-DKU_DEBUG>
  )

  target_compile_options(kubik PRIVATE
    $<$<CONFIG:Debug>:-O0 -g3>
    # See if this will still work on all platforms, never really tested this before
    $<$<CONFIG:Release>:-O3 -flto -ffast-math>
)

if(NOT WIN32)
    target_link_libraries(kubik dl)
endif()