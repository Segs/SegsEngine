if(${OPTION_PLATFORM} STREQUAL "server")
    return()
endif()

set(gles3_shader_sources)
# FIXME: detect GLES3
set(shader_sources
    copy.glsl
    resolve.glsl
    canvas.glsl
    canvas_shadow.glsl
    scene.glsl
    cubemap_filter.glsl
    cube_to_dp.glsl
    blend_shape.glsl
    lens_distorted.glsl
    screen_space_reflection.glsl
    effect_blur.glsl
    subsurf_scattering.glsl
    ssao.glsl
    ssao_minify.glsl
    ssao_blur.glsl
    exposure.glsl
    tonemap.glsl
    particles.glsl
)
foreach(shader ${shader_sources})
    add_custom_command(
        COMMAND shader_to_header 3 ${CMAKE_CURRENT_SOURCE_DIR}/${shader}
        OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/${shader}.gen.h
        DEPENDS ${shader} shader_to_header
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating shader include for ${shader}"
    )
    set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/${shader}.gen.h PROPERTIES GENERATED TRUE)
    list(APPEND gles3_shader_sources ${CMAKE_CURRENT_BINARY_DIR}/${shader}.gen.h)
endforeach()
add_library(gles3_shaders OBJECT ${gles3_shader_sources} ${shader_sources} gen_helper.cpp)
target_include_directories(gles3_shaders PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/..)
