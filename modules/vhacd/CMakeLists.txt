file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

set(thirdparty_sources
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/vhacdManifoldMesh.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/FloatMath.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/vhacdMesh.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/vhacdICHull.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/vhacdVolume.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/VHACD-ASYNC.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/btAlignedAllocator.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/vhacdRaycastMesh.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/VHACD.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/src/btConvexHullComputer.cpp
)

list(APPEND thirdparty_sources ${plat_source_files} ${plat_header_files})

foreach(tgt ${global_targets})
    add_library(${tgt}_vhacd OBJECT ${source_files} ${header_files} ${thirdparty_sources})

    target_include_directories(${tgt}_vhacd PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/vhacd/inc)
    target_link_libraries(${tgt}_vhacd PUBLIC ${tgt}_core)
    set_common_target_properties(${tgt}_vhacd)
endforeach()
