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
target_include_directories(zlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )

#set_common_target_properties(zlib)
