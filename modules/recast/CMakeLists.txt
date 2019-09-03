file(GLOB source_files "*.cpp" )
file(GLOB header_files "*.h" )

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
foreach(tgt ${global_targets})
    add_library(${tgt}_recast OBJECT ${source_files} ${header_files} ${recast_source_files})

    target_compile_definitions(${tgt}_recast PUBLIC -DRECAST_ENABLED)
    target_include_directories(${tgt}_recast PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Include)

    target_link_libraries(${tgt}_recast PUBLIC ${tgt}_core)
    set_common_target_properties(${tgt}_recast)
endforeach()
