

add_library(glad STATIC ${CMAKE_CURRENT_SOURCE_DIR}/src/gl.c)
target_include_directories(glad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(glad PRIVATE -w)

set_target_properties(glad PROPERTIES LINKER_LANGUAGE C)
set_target_properties(glad PROPERTIES FOLDER deps)
