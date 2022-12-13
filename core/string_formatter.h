#pragma once
#include "core/string.h"
//#include "thirdparty/fmt/fmt/format.h"
//#include "thirdparty/fmt/fmt/core.h"


////inline fmt::basic_string_view<char16_t> to_string_view(const String& s) {
////  return {s.ptr(), s.length()};
////}
//QT_BEGIN_NAMESPACE
//inline fmt::basic_string_view<char16_t> to_string_view(const QString & String) noexcept {
//  return { reinterpret_cast<const char16_t *>(String.data()), static_cast<size_t>(String.length()) };
//}
//inline fmt::basic_string_view<char> to_string_view(const QByteArray & String) noexcept {
//    return { reinterpret_cast<const char *>(String.data()), static_cast<size_t>(String.length()) };
//}
//QT_END_NAMESPACE
//inline fmt::basic_string_view<char16_t> to_string_view(const String & str) noexcept {
//    return { reinterpret_cast<const char16_t *>(str.ptr()), static_cast<size_t>(str.length()) };
//}

//template <>
//struct fmt::formatter<QString, char16_t> {
//  template <typename ParseContext>
//  constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

//  template <typename FormatContext>
//  auto format(const QString &v, FormatContext &ctx) {
//    return format_to(ctx.begin(), u"{}", v.utf16());
//  }
//};

//template <>
//struct fmt::formatter<String, char16_t> {
//  template <typename ParseContext>
//  constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

//  template <typename FormatContext>
//  auto format(const String &v, FormatContext &ctx) {
//    return format_to(ctx.begin(), u"{}", v.ptr());
//  }
//};

//using FmtBuffer = fmt::basic_memory_buffer<char,500>;
//using FmtBuffer16 = fmt::basic_memory_buffer<char16_t,500>;
//template <typename... Args>
//inline String FormatV(const char *formatString, const Args &... args)
//{
//    String ret;
//    fmt::format_to(std::back_inserter(ret), formatString, args...);
//    return ret;
//}
//template <typename... Args>
//inline CharString Format(const char *formatString, const Args &... args)
//{
//    CharString ret;
//    fmt::format_to(std::back_inserter(ret), formatString, args...);
//    return ret;
//}

#define FormatVE(FMT,...) String(String::CtorSprintf(),FMT,__VA_ARGS__)
#define FormatSN(FMT,...) StringName(String(String::CtorSprintf(),FMT,__VA_ARGS__))
