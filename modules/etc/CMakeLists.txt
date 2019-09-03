macro(remove_flag_from_target _target _flag)
    get_target_property(_target_cxx_flags ${_target} COMPILE_OPTIONS)
    if(_target_cxx_flags)
        list(REMOVE_ITEM _target_cxx_flags ${_flag})
        set_target_properties(${_target} PROPERTIES COMPILE_OPTIONS "${_target_cxx_flags}")
    endif()
endmacro()

set(source_files
    ${PROJECT_SOURCE_DIR}/thirdparty/etc2comp/EtcBlock4x4.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/etc2comp/EtcBlock4x4Encoding.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/etc2comp/EtcBlock4x4Encoding_ETC1.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/etc2comp/EtcBlock4x4Encoding_R11.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/etc2comp/EtcBlock4x4Encoding_RG11.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/etc2comp/EtcBlock4x4Encoding_RGB8A1.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/etc2comp/EtcBlock4x4Encoding_RGB8.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/etc2comp/EtcBlock4x4Encoding_RGBA8.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/etc2comp/Etc.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/etc2comp/EtcDifferentialTrys.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/etc2comp/EtcFilter.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/etc2comp/EtcImage.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/etc2comp/EtcIndividualTrys.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/etc2comp/EtcMath.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/etc2comp/EtcSortedBlockList.cpp
)


foreach(tgt ${global_targets})
    add_library(${tgt}_etc OBJECT ${source_files}
        image_etc.cpp
        image_etc.h
        register_types.cpp
        register_types.h
        texture_loader_pkm.cpp
        texture_loader_pkm.h
    )
    target_include_directories(${tgt}_etc PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/etc2comp/)

    target_link_libraries(${tgt}_etc PUBLIC ${tgt}_core)
    set_common_target_properties(${tgt}_etc)
    remove_flag_from_target(${tgt}_etc -ffast-math)
endforeach()
