#pragma once

#if _WIN32
/* We are building this library */
#define EXPORT_PREFIX __declspec(dllexport)
#define EXPORT_PREFIX_OLD EXPORT_PREFIX
/* We are using this library */
#define IMPORT_PREFIX __declspec(dllimport)
#define IMPORT_PREFIX_OLD IMPORT_PREFIX
#else
#define EXPORT_PREFIX [[gnu::visibility("default")]]
#define EXPORT_PREFIX_OLD __attribute__((visibility("default")))
#define IMPORT_PREFIX [[gnu::visibility("default")]]
#define IMPORT_PREFIX_OLD __attribute__((visibility("default")))
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
#        define GODOT_EXPORT IMPORT_PREFIX
#    endif
#  endif

#  ifndef GODOT_NO_EXPORT
#if _WIN32
#    define GODOT_NO_EXPORT
#else
#    define GODOT_NO_EXPORT [[gnu::visibility("hidden")]]
#endif
#  endif
#endif

#ifndef GODOT_DEPRECATED
#if _WIN32
#  define GODOT_DEPRECATED __declspec(deprecated)
#else
#  define GODOT_DEPRECATED [[deprecated]]
#endif
#endif

#ifndef GODOT_DEPRECATED_EXPORT
#  define GODOT_DEPRECATED_EXPORT GODOT_EXPORT GODOT_DEPRECATED
#endif

#ifndef GODOT_DEPRECATED_NO_EXPORT
#  define GODOT_DEPRECATED_NO_EXPORT GODOT_NO_EXPORT GODOT_DEPRECATED
#endif

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE.BSD file.

// In a header file, write:
//
//   extern template class EXPORT_TEMPLATE_DECLARE(FOO_EXPORT) foo<bar>;
//
// In a source file, write:
//
//   template class EXPORT_TEMPLATE_DEFINE(FOO_EXPORT) foo<bar>;
// Implementation notes
//
// On Windows, when building the FOO library (that is, when FOO_EXPORT expands
// to __declspec(dllexport)), we want the two lines to expand to:
//
//     extern template class foo<bar>;
//     template class FOO_EXPORT foo<bar>;
//
// In all other cases (non-Windows, and Windows when using the FOO library (that
// is when FOO_EXPORT expands to __declspec(dllimport)), we want:
//
//     extern template class FOO_EXPORT foo<bar>;
//     template class foo<bar>;
#ifdef _MSC_VER
#   define EXPORT_TEMPLATE_DECLARE(X)
#   define EXPORT_TEMPLATE_DEFINE(X) X
#else
#   define EXPORT_TEMPLATE_DECLARE(X) X
#   define EXPORT_TEMPLATE_DEFINE(X)
#endif

#define EXPORT_TEMPLATE_DECL EXPORT_TEMPLATE_DECLARE(EXPORT_PREFIX_OLD)

#define GODOT_TEMPLATE_EXT_DEFINE(X) template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) X;
#define GODOT_TEMPLATE_EXT_DECLARE(X) extern template class EXPORT_TEMPLATE_DECL X;
