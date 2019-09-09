#pragma once

#if _WIN32
/* We are building this library */
#define EXPORT_PREFIX __declspec(dllexport)
#else
#define EXPORT_PREFIX __attribute__((visibility("default")))
#endif

#ifdef _MSC_VER
#define GODOT_EXPORT_TEMPLATE_A
#define GODOT_EXPORT_TEMPLATE_B __declspec(dllexport)
#else
#define GODOT_EXPORT_TEMPLATE_A __attribute__((visibility("default")))
#define GODOT_EXPORT_TEMPLATE_B
#endif
#ifdef GODOT_STATIC_DEFINE
#  define GODOT_EXPORT
#  define GODOT_NO_EXPORT
#else
#  ifndef GODOT_EXPORT
#    ifdef GODOT_EXPORTS
/* We are building this library */
#define GODOT_EXPORT EXPORT_PREFIX
#    else
#       if _WIN32
/* We are using this library */
#           define GODOT_EXPORT __declspec(dllimport)
#       else
#           define GODOT_EXPORT __attribute__((visibility("default")))
#       endif
#    endif
#  endif

#  ifndef GODOT_NO_EXPORT
#if _WIN32
#    define GODOT_NO_EXPORT
#else
#    define GODOT_NO_EXPORT __attribute__((visibility("hidden")))
#endif
#  endif
#endif

#ifndef GODOT_DEPRECATED
#if _WIN32
#  define GODOT_DEPRECATED __declspec(deprecated)
#else
#  define GODOT_DEPRECATED __attribute__ ((__deprecated__))
#endif
#endif

#ifndef GODOT_DEPRECATED_EXPORT
#  define GODOT_DEPRECATED_EXPORT GODOT_EXPORT GODOT_DEPRECATED
#endif

#ifndef GODOT_DEPRECATED_NO_EXPORT
#  define GODOT_DEPRECATED_NO_EXPORT GODOT_NO_EXPORT GODOT_DEPRECATED
#endif

#define GODOT_TEMPLATE_EXT_DEFINE(X) template class GODOT_EXPORT_TEMPLATE_B X;
#define GODOT_TEMPLATE_EXT_DECLARE(X) extern template class GODOT_EXPORT_TEMPLATE_A X;
