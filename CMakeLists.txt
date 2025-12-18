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

include_directories(${LIB_DIR})
add_subdirectory(${LIB_DIR}/glad)
include(${CMAKE_SOURCE_DIR}/cmake/glfw.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/cimgui.cmake)

# Main executable
set(SRC_FILES
  src/main.c
  src/maf.c
  src/player.c
  src/gfx.c
  src/chunk.c
  src/world.c
  src/mesh.c
  src/thread.c
  src/load.c
  src/queue.c
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
    $<$<CONFIG:Release>:-O3 -flto -ffast-math>
)

if(NOT WIN32)
    target_link_libraries(kubik dl)
endif()