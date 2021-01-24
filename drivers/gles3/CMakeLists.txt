add_subdirectory(shaders)

file(GLOB source_files "*.cpp")
file(GLOB header_files "*.h;*.inc")
#env["check_c_headers"] = [ [ "mntent.h", "HAVE_MNTENT" ] ]
target_sources(${tgt}_drivers PRIVATE ${source_files} ${header_files})
