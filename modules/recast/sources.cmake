file(GLOB source_files "recast/*.cpp")
file(GLOB header_files "recast/*.h")
set(recast_source_files
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/Recast.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastAlloc.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastArea.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastAssert.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastContour.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastFilter.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastLayers.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastMesh.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastMeshDetail.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastRasterization.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/RecastRegion.cpp

)

target_sources(${tgt}_modules PRIVATE ${source_files} ${header_files} ${recast_source_files})
target_include_directories(${tgt}_modules PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Include)
target_compile_definitions(${tgt}_modules PUBLIC MODULE_RECAST_ENABLED RECAST_ENABLED)
