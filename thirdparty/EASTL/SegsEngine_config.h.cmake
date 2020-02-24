#pragma once

#if _WIN32
#   define EASTL_EXPORT_PREFIX __declspec(dllexport)
#   define EASTL_IMPORT_PREFIX __declspec(dllimport)
#else
#   define EASTL_EXPORT_PREFIX __attribute__((visibility("default")))
#   define EASTL_IMPORT_PREFIX __attribute__((visibility("default")))
#endif

#ifdef GODOT_STATIC_DEFINE
#  define EASTL_API
#else
#  ifndef GODOT_EXPORT
#    ifdef GODOT_EXPORTS
#       define EASTL_API EASTL_EXPORT_PREFIX
#    else
#       define EASTL_API EASTL_IMPORT_PREFIX
#    endif
#  endif
#endif

#ifndef LUTEFISK3D_STATIC_DEFINE
#  define EASTL_DLL 1
#endif
#define EASTL_LF3D_EXTENSIONS 1

#define EA_PRAGMA_ONCE_SUPPORTED
#define EA_NOEXCEPT noexcept
#define EA_NOEXCEPT_IF(predicate) noexcept((predicate))
#define EA_NOEXCEPT_EXPR(expression) noexcept((expression))
#define EA_COMPILER_CPP11_ENABLED 1
#define EA_COMPILER_CPP14_ENABLED 1
#define EA_CPP14_CONSTEXPR constexpr
#define EASTL_EASTDC_VSNPRINTF 0
#define EASTL_DEFAULT_NAME_PREFIX "SegsEngine"
#define EASTL_ALLOCATOR_EXPLICIT_ENABLED 1
#define EASTL_VARIADIC_TEMPLATES_ENABLED 1
#define EASTL_OPENSOURCE 1
#define EASTL_HAVE_CPP11_TYPE_TRAITS 1
#define EASTL_VARIABLE_TEMPLATES_ENABLED 1
#define EASTL_RESET_ENABLED 1
#define EASTLAddRef AddRef
#define EASTLRelease ReleaseRef

