set(thirdparty_zstd_dir "${PROJECT_SOURCE_DIR}/thirdparty/zstd/")
set(thirdparty_zstd_sources
    common/bitstream.h
    common/cpu.h
    common/debug.h
    common/error_private.c
    common/fse_decompress.c
    common/huf.h
    common/pool.c
    common/threading.c
    common/xxhash.c
    common/zstd_common.c
    common/zstd_internal.h
    common/compiler.h
    common/debug.c
    common/entropy_common.c
    common/error_private.h
    common/fse.h
    common/mem.h
    common/pool.h
    common/threading.h
    common/xxhash.h
    common/zstd_errors.h


    compress/fse_compress.c
    compress/huf_compress.c
    compress/zstd_compress_literals.c
    compress/zstd_compress_sequences.h
    compress/zstd_cwksp.h
    compress/zstd_fast.c
    compress/zstd_lazy.h
    compress/zstdmt_compress.c
    compress/zstd_opt.h
    compress/hist.c
    compress/zstd_compress.c
    compress/zstd_compress_literals.h
    compress/zstd_compress_superblock.c
    compress/zstd_double_fast.c
    compress/zstd_fast.h
    compress/zstd_ldm.c
    compress/zstdmt_compress.h
    compress/hist.h
    compress/zstd_compress_internal.h
    compress/zstd_compress_sequences.c
    compress/zstd_compress_superblock.h
    compress/zstd_double_fast.h
    compress/zstd_lazy.c
    compress/zstd_ldm.h
    compress/zstd_opt.c


    decompress/huf_decompress.c
    decompress/zstd_ddict.c
    decompress/zstd_ddict.h
    decompress/zstd_decompress_block.c
    decompress/zstd_decompress_block.h
    decompress/zstd_decompress.c
    decompress/zstd_decompress_internal.h
)
add_library(zstd_internal STATIC ${thirdparty_zstd_sources})
target_include_directories(zstd_internal PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/zstd/ ${PROJECT_SOURCE_DIR}/thirdparty/zstd/common)
target_compile_definitions(zstd_internal PUBLIC ZSTD_STATIC_LINKING_ONLY)
set_property(TARGET zstd_internal PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(zstd_internal PROPERTIES FOLDER 3rd_party)
