# env_thirdparty.add_source_files(env.modules_sources, Glob(''))
file(GLOB assimp_source_files1 "${PROJECT_SOURCE_DIR}/thirdparty/assimp/code/Common/*.cpp" )
file(GLOB assimp_source_files2 "${PROJECT_SOURCE_DIR}/thirdparty/assimp/code/PostProcessing/*.cpp")
file(GLOB assimp_source_files3 "${PROJECT_SOURCE_DIR}/thirdparty/assimp/code/Material/*.cpp")
file(GLOB assimp_source_files4 "${PROJECT_SOURCE_DIR}/thirdparty/assimp/code/FBX/*.cpp")
file(GLOB assimp_source_files5 "${PROJECT_SOURCE_DIR}/thirdparty/assimp/code/MMD/*.cpp")
file(GLOB assimp_source_files6 "${PROJECT_SOURCE_DIR}/thirdparty/assimp/code/glTF/*.cpp")
file(GLOB assimp_source_files7 "${PROJECT_SOURCE_DIR}/thirdparty/assimp/code/glTF2/*.cpp")

add_library(assimp_lib OBJECT
    ${assimp_source_files1}
    ${assimp_source_files2}
    ${assimp_source_files3}
    ${assimp_source_files4}
    ${assimp_source_files5}
    ${assimp_source_files6}
    ${assimp_source_files7}
)
target_include_directories(assimp_lib PRIVATE
    ${PROJECT_SOURCE_DIR}/thirdparty/assimp
    ${PROJECT_SOURCE_DIR}/thirdparty/assimp/include
    ${PROJECT_SOURCE_DIR}/thirdparty/assimp/code/Importer/IFC
    ${PROJECT_SOURCE_DIR}/thirdparty/misc
    ${PROJECT_SOURCE_DIR}/thirdparty/assimp/code
    ${PROJECT_SOURCE_DIR}/thirdparty/assimp/common
    ${PROJECT_SOURCE_DIR}/thirdparty/assimp/contrib/irrXML/
    ${PROJECT_SOURCE_DIR}/thirdparty/assimp/contrib/unzip/
    ${PROJECT_SOURCE_DIR}/thirdparty/assimp/code/Importer/STEPParser
    ${PROJECT_SOURCE_DIR}/thirdparty/assimp/
    ${PROJECT_SOURCE_DIR}/thirdparty/zlib/
    ${PROJECT_SOURCE_DIR}/thirdparty/assimp/contrib/openddlparser/include
    ${PROJECT_SOURCE_DIR}/thirdparty/assimp/contrib/rapidjson/include
)
# using SYSTEM to reduce amount of warnings from assimp code.
target_include_directories(assimp_lib SYSTEM INTERFACE
    ${PROJECT_SOURCE_DIR}/thirdparty/assimp
    ${PROJECT_SOURCE_DIR}/thirdparty/assimp/include
)
target_compile_definitions(assimp_lib PRIVATE
    #ASSIMP_DOUBLE_PRECISION # TODO default to what godot is compiled with for future double support
    ASSIMP_BUILD_BOOST_WORKAROUND
    OPENDDLPARSER_BUILD
    ASSIMP_BUILD_NO_OWN_ZLIB
    ASSIMP_BUILD_NO_EXPORT
    ASSIMP_BUILD_NO_X_IMPORTER
    ASSIMP_BUILD_NO_AMF_IMPORTER
    ASSIMP_BUILD_NO_3DS_IMPORTER
    ASSIMP_BUILD_NO_MD3_IMPORTER
    ASSIMP_BUILD_NO_MD5_IMPORTER
    ASSIMP_BUILD_NO_MDL_IMPORTER
    ASSIMP_BUILD_NO_MD2_IMPORTER
    ASSIMP_BUILD_NO_PLY_IMPORTER
    ASSIMP_BUILD_NO_ASE_IMPORTER
    ASSIMP_BUILD_NO_OBJ_IMPORTER
    ASSIMP_BUILD_NO_HMP_IMPORTER
    ASSIMP_BUILD_NO_SMD_IMPORTER
    ASSIMP_BUILD_NO_MDC_IMPORTER
    ASSIMP_BUILD_NO_MD5_IMPORTER
    ASSIMP_BUILD_NO_STL_IMPORTER
    ASSIMP_BUILD_NO_LWO_IMPORTER
    ASSIMP_BUILD_NO_DXF_IMPORTER
    ASSIMP_BUILD_NO_NFF_IMPORTER
    ASSIMP_BUILD_NO_RAW_IMPORTER
    ASSIMP_BUILD_NO_SIB_IMPORTER
    ASSIMP_BUILD_NO_OFF_IMPORTER
    ASSIMP_BUILD_NO_AC_IMPORTER
    ASSIMP_BUILD_NO_BVH_IMPORTER
    ASSIMP_BUILD_NO_IRRMESH_IMPORTER
    ASSIMP_BUILD_NO_IRR_IMPORTER
    ASSIMP_BUILD_NO_Q3D_IMPORTER
    ASSIMP_BUILD_NO_B3D_IMPORTER
    ASSIMP_BUILD_NO_COLLADA_IMPORTER
    ASSIMP_BUILD_NO_TERRAGEN_IMPORTER
    ASSIMP_BUILD_NO_CSM_IMPORTER
    ASSIMP_BUILD_NO_3D_IMPORTER
    ASSIMP_BUILD_NO_LWS_IMPORTER
    ASSIMP_BUILD_NO_OGRE_IMPORTER
    ASSIMP_BUILD_NO_OPENGEX_IMPORTER
    ASSIMP_BUILD_NO_MS3D_IMPORTER
    ASSIMP_BUILD_NO_COB_IMPORTER
    ASSIMP_BUILD_NO_BLEND_IMPORTER
    ASSIMP_BUILD_NO_Q3BSP_IMPORTER
    ASSIMP_BUILD_NO_NDO_IMPORTER
    ASSIMP_BUILD_NO_STEP_IMPORTER
    ASSIMP_BUILD_NO_IFC_IMPORTER
    ASSIMP_BUILD_NO_XGL_IMPORTER
    ASSIMP_BUILD_NO_ASSBIN_IMPORTER
    ASSIMP_BUILD_NO_C4D_IMPORTER
    ASSIMP_BUILD_NO_3MF_IMPORTER
    ASSIMP_BUILD_NO_X3D_IMPORTER
    ASSIMP_BUILD_NO_GLTF_IMPORTER
    ASSIMP_BUILD_NO_GLTF2_IMPORTER
    ASSIMP_BUILD_SINGLETHREADED
)
if((${OPTION_PLATFORM} STREQUAL "windows") OR (${OPTION_PLATFORM} STREQUAL "uwp")) # Winsock
    target_compile_definitions(assimp_lib PRIVATE PLATFORM_WINDOWS PLATFORM=WINDOWS)
elseif(${OPTION_PLATFORM} STREQUAL "osx") #
    target_compile_definitions(assimp_lib PRIVATE PLATFORM_DARWIN PLATFORM=DARWIN)
else()
    target_compile_definitions(assimp_lib PRIVATE PLATFORM_LINUX PLATFORM=LINUX)
endif()
set_property(TARGET assimp_lib PROPERTY POSITION_INDEPENDENT_CODE ON)
set_common_target_properties(assimp_lib)
