target_sources(${tgt}_drivers PRIVATE
    ${PROJECT_SOURCE_DIR}/thirdparty/glad/glad.c
)

target_include_directories(${tgt}_drivers PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/glad/)
target_compile_definitions(${tgt}_drivers PUBLIC GLAD_ENABLED )
