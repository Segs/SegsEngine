# Thirdparty dependencies
set(thirdparty_sources
    ${PROJECT_SOURCE_DIR}/thirdparty/b2d_convexdecomp/b2Polygon.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/b2d_convexdecomp/b2Triangle.cpp
)

target_sources(${tgt}_drivers PRIVATE
    ${thirdparty_sources}
    b2d_decompose.cpp
    b2d_decompose.h
)
target_include_directories(${tgt}_drivers PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/b2d_convexdecomp)
