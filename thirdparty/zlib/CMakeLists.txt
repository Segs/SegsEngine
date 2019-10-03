# Thirdparty source files
# No check here as already done in drivers/SCsub
# Thirdparty code
set(thirdparty_sources
    adler32.c
    compress.c
    crc32.c
    deflate.c
    infback.c
    inffast.c
    inflate.c
    inftrees.c
    trees.c
    uncompr.c
    zutil.c
)
add_library(zlib STATIC ${thirdparty_sources})
# interface library target is for the usage of zlib in plugins etc.
add_library(zlib_interface INTERFACE)
target_include_directories(zlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )
target_include_directories(zlib_interface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} )
# tell zlib to export it's symbols, that way plugins will not have to carry their own zlib.
target_compile_definitions(zlib PUBLIC ZLIB_DLL )
target_compile_definitions(zlib PUBLIC ZLIB_INTERNAL)
target_compile_definitions(zlib_interface INTERFACE ZLIB_DLL )

if(NOT MSVC)
    target_compile_definitions(zlib PUBLIC HAVE_HIDDEN )
    target_compile_definitions(zlib_interface INTERFACE HAVE_HIDDEN )
endif()
#set_common_target_properties(zlib)
set_property(TARGET zlib PROPERTY POSITION_INDEPENDENT_CODE ON)
