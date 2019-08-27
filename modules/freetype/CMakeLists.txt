set(source_files
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/autofit/autofit.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftbase.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftbbox.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftbdf.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftbitmap.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftcid.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftdebug.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftfstype.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftgasp.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftglyph.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftgxval.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftinit.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftmm.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftotval.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftpatent.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftpfr.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftstroke.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftsynth.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftsystem.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/fttype1.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/base/ftwinfnt.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/bdf/bdf.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/bzip2/ftbzip2.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/cache/ftcache.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/cff/cff.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/cid/type1cid.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/gxvalid/gxvalid.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/gzip/ftgzip.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/lzw/ftlzw.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/otvalid/otvalid.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/pcf/pcf.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/pfr/pfr.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/psaux/psaux.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/pshinter/pshinter.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/psnames/psnames.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/raster/raster.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/smooth/smooth.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/truetype/truetype.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/type1/type1.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/type42/type42.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/winfonts/winfnt.c
    ${PROJECT_SOURCE_DIR}/thirdparty/freetype/src/sfnt/sfnt.c

)

foreach(tgt ${global_targets})
    add_library(${tgt}_freetype OBJECT ${source_files} register_types.cpp register_types.h)

    target_compile_definitions(${tgt}_freetype PRIVATE FT2_BUILD_LIBRARY)
    target_compile_definitions(${tgt}_freetype PUBLIC FREETYPE_ENABLED FT_CONFIG_OPTION_USE_PNG)
    if(OPTION_BUILTIN_LIBPNG)
        target_include_directories(${tgt}_freetype PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/libpng)
    endif()
    target_include_directories(${tgt}_freetype PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/freetype/include)

    target_link_libraries(${tgt}_freetype PUBLIC ${tgt}_core)
    target_link_libraries(${tgt}_freetype PRIVATE zlib) # to propagate zlib things here.
    set_common_target_properties(${tgt}_freetype)
endforeach()
