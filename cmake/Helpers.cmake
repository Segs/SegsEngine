# This defines target properties common to all engine components
macro(set_common_target_properties TARGET)
    set_target_properties(${TARGET} PROPERTIES
        CXX_VISIBILITY_PRESET hidden # -fvisibility=hidden
        C_VISIBILITY_PRESET hidden # -fvisibility=hidden
        VISIBILITY_INLINES_HIDDEN TRUE
    )
    # always export symbols marked as such
    target_compile_definitions(${TARGET} PRIVATE
        $<BUILD_INTERFACE:GODOT_EXPORTS>
    )
    if(USE_TRACY_PROFILER)
        target_compile_definitions(${TARGET} PRIVATE TRACY_ENABLE TRACY_ON_DEMAND)
        target_link_libraries(${TARGET} PUBLIC Threads::Threads)
    endif()
endmacro()

# Function to wrap a given string into multiple lines at the given column position.
# Parameters:
#   VARIABLE    - The name of the CMake variable holding the string.
#   AT_COLUMN   - The column position at which string will be wrapped.
function(WRAP_STRING)
    set(oneValueArgs VARIABLE AT_COLUMN)
    cmake_parse_arguments(WRAP_STRING "${options}" "${oneValueArgs}" "" ${ARGN})

    string(LENGTH ${${WRAP_STRING_VARIABLE}} stringLength)
    math(EXPR offset "0")
    set(lines "")
    while(stringLength GREATER 0)

        if(stringLength GREATER ${WRAP_STRING_AT_COLUMN})
            math(EXPR length "${WRAP_STRING_AT_COLUMN}")
        else()
            math(EXPR length "${stringLength}")
        endif()

        string(SUBSTRING ${${WRAP_STRING_VARIABLE}} ${offset} ${length} line)
        set(lines "${lines}\n${line}")

        math(EXPR stringLength "${stringLength} - ${length}")
        math(EXPR offset "${offset} + ${length}")
    endwhile()

    set(${WRAP_STRING_VARIABLE} "${lines}" PARENT_SCOPE)
endfunction()

# Function to embed contents of a file as byte array in C/C++ header file(.h). The header file
# will contain a byte array and integer variable holding the size of the array.
# Parameters
#   TARGET_VAR      - Will be filled with hex-contents
#   SOURCE_FILE     - The path of source file whose contents will be embedded in the header file.
#   NULL_TERMINATE  - If specified a null byte(zero) will be append to the byte array. This will be
#                     useful if the source file is a text file and we want to use the file contents
#                     as string. But the size variable holds size of the byte array without this
#                     null byte.
# Usage:
#   BIN2HexArray(SOURCE_FILE "Logo.png" TARGET_VAR "Var")
function(BIN2HexArray)
    set(options NULL_TERMINATE)
    set(oneValueArgs SOURCE_FILE TARGET_VAR)
    cmake_parse_arguments(BIN2H "${options}" "${oneValueArgs}" "" ${ARGN})

    # reads source file contents as hex string
    file(READ ${BIN2H_SOURCE_FILE} hexString HEX)
    string(LENGTH ${hexString} hexStringLength)

    # appends null byte if asked
    if(BIN2H_NULL_TERMINATE)
        set(hexString "${hexString}00")
    endif()

    # wraps the hex string into multiple lines at column 32(i.e. 16 bytes per line)
    wrap_string(VARIABLE hexString AT_COLUMN 32)
    math(EXPR arraySize "${hexStringLength} / 2")

    # adds '0x' prefix and comma suffix before and after every byte respectively
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " arrayValues ${hexString})
    # removes trailing comma
    string(REGEX REPLACE ", $" "" arrayValues ${arrayValues})
    set(${BIN2H_TARGET_VAR} ${arrayValues} PARENT_SCOPE)
    # converts the variable name into proper C identifier
endfunction()

# Function to embed contents of a file as byte array in C/C++ header file(.h). The header file
# will contain a byte array and integer variable holding the size of the array.
# Parameters
#   TARGET_VAR      - Will be filled with hex-contents
#   SOURCE_FILE     - The path of source file whose contents will be embedded in the header file.
#   VARIABLE_NAME   - The name of the variable for the byte array. The string "_SIZE" will be append
#                     to this name and will be used a variable name for size variable.
#   HEADER_FILE     - The path of header file.
#   APPEND          - If specified appends to the header file instead of overwriting it
#   NULL_TERMINATE  - If specified a null byte(zero) will be append to the byte array. This will be
#                     useful if the source file is a text file and we want to use the file contents
#                     as string. But the size variable holds size of the byte array without this
#                     null byte.
# Usage:
#   bin2h(SOURCE_FILE "Logo.png" HEADER_FILE "Logo.h" VARIABLE_NAME "LOGO_PNG")
function(BIN2H)
    set(options APPEND NULL_TERMINATE)
    set(oneValueArgs SOURCE_FILE VARIABLE_NAME HEADER_FILE)
    cmake_parse_arguments(BIN2H "${options}" "${oneValueArgs}" "" ${ARGN})

    # reads source file contents as hex string
    file(READ ${BIN2H_SOURCE_FILE} hexString HEX)
    string(LENGTH ${hexString} hexStringLength)

    # appends null byte if asked
    if(BIN2H_NULL_TERMINATE)
        set(hexString "${hexString}00")
    endif()

    # wraps the hex string into multiple lines at column 32(i.e. 16 bytes per line)
    wrap_string(VARIABLE hexString AT_COLUMN 32)
    math(EXPR arraySize "${hexStringLength} / 2")

    # adds '0x' prefix and comma suffix before and after every byte respectively
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " arrayValues ${hexString})
    # removes trailing comma
    string(REGEX REPLACE ", $" "" arrayValues ${arrayValues})

    # converts the variable name into proper C identifier
    string(MAKE_C_IDENTIFIER "${BIN2H_VARIABLE_NAME}" BIN2H_VARIABLE_NAME)
    #string(TOUPPER "${BIN2H_VARIABLE_NAME}" BIN2H_VARIABLE_NAME)

    # declares byte array and the length variables
    set(arrayDefinition "static const unsigned char ${BIN2H_VARIABLE_NAME}[] = { ${arrayValues} };")
    set(declarations "${arrayDefinition}\n\n")
    if(BIN2H_APPEND)
        file(APPEND ${BIN2H_HEADER_FILE} "${declarations}")
    else()
        file(WRITE ${BIN2H_HEADER_FILE} "${declarations}")
    endif()
endfunction()

function(save_active_platforms apnames ap)
    foreach(x ${${ap}})
        set(names "logo")
        get_filename_component(base_path ${x} NAME)
        foreach(name ${names})
            bin2h(SOURCE_FILE "${x}/${name}.png" HEADER_FILE "${x}/${name}.gen.h" VARIABLE_NAME "_${base_path}_${name}")
        endforeach()
    endforeach()
endfunction()

function(update_version module_version_string)
    set(VERSION_BUILD "custom_build")
    if ($ENV{BUILD_NAME})
        set(VERSION_BUILD $ENV{BUILD_NAME})
    endif()
    # for version and revision info. Derived from https://github.com/dolphin-emu/dolphin/blob/master/CMakeLists.txt#L132
    find_package(Git)

    include(../version.cmake)
    string(TIMESTAMP VERSION_YEAR "%Y")
    configure_file(version_generated.h.cmake ${CMAKE_CURRENT_LIST_DIR}/version_generated.gen.h)
    set(VERSION_HASH "")
    set(VERSION_WEBSITE "https://godotengine.org")
    if(GIT_FOUND)
        # defines SEGS_REVISION
        execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
            OUTPUT_VARIABLE SEGS_REVISION
            OUTPUT_STRIP_TRAILING_WHITESPACE)
            set(VERSION_HASH ${SEGS_REVISION})
    endif()
    configure_file(version_hash.cpp.cmake version_hash.gen.cpp )
endfunction()

macro(add_object_lib dirname)
    if(NOT TARGET ${dirname})
        add_subdirectory(${dirname})
        get_property(${dirname}_include_dirs TARGET ${dirname} PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
        get_property(${dirname}_defs TARGET ${dirname} PROPERTY INTERFACE_COMPILE_DEFINITIONS)
        set_common_target_properties(${dirname})
    endif()
endmacro()

macro(use_object_lib in_target objectlib_name)
    target_sources(${in_target} PRIVATE $<TARGET_OBJECTS:${objectlib_name}>)
    if(TARGET ${objectlib_name}_interface)
        target_link_libraries(${in_target} PUBLIC ${objectlib_name}_interface) # pull in things an OBJECT library is not allowed to have, mostly link targets
    endif()
    target_compile_definitions(${in_target} PUBLIC ${${objectlib_name}_defs})
    target_include_directories(${in_target} PUBLIC ${${objectlib_name}_include_dirs})
endmacro()
