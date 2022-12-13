/*************************************************************************/
/*  editor_property_name_processor.cpp                                   */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "editor_property_name_processor.h"

#include "core/string_utils.h"
#include "core/string_utils.inl"
#include "core/translation_helpers.h"
#include "editor/editor_settings.h"

namespace {
bool names_initialized = false;
Map<String, String> capitalize_string_cache;
Map<String, String> capitalize_string_remaps;

void init_names() {
    if (names_initialized)
        return;
    names_initialized = true;
    // The following initialization is parsed in `editor/translations/extract.py` with a regex.
    // The map name and value definition format should be kept synced with the regex.
    capitalize_string_remaps["2d"] = "2D";
    capitalize_string_remaps["3d"] = "3D";
    capitalize_string_remaps["aa"] = "AA";
    capitalize_string_remaps["aabb"] = "AABB";
    capitalize_string_remaps["adb"] = "ADB";
    capitalize_string_remaps["ao"] = "AO";
    capitalize_string_remaps["apk"] = "APK";
    capitalize_string_remaps["arm64-v8a"] = "arm64-v8a";
    capitalize_string_remaps["armeabi-v7a"] = "armeabi-v7a";
    capitalize_string_remaps["arvr"] = "ARVR";
    capitalize_string_remaps["bg"] = "BG";
    capitalize_string_remaps["bp"] = "BP";
    capitalize_string_remaps["bpc"] = "BPC";
    capitalize_string_remaps["bptc"] = "BPTC";
    capitalize_string_remaps["bvh"] = "BVH";
    capitalize_string_remaps["ca"] = "CA";
    capitalize_string_remaps["cd"] = "CD";
    capitalize_string_remaps["commentfocus"] = "Comment Focus";
    capitalize_string_remaps["cpu"] = "CPU";
    capitalize_string_remaps["csg"] = "CSG";
    capitalize_string_remaps["db"] = "dB";
    capitalize_string_remaps["defaultfocus"] = "Default Focus";
    capitalize_string_remaps["defaultframe"] = "Default Frame";
    capitalize_string_remaps["dof"] = "DoF";
    capitalize_string_remaps["dpi"] = "DPI";
    capitalize_string_remaps["dtls"] = "DTLS";
    capitalize_string_remaps["erp"] = "ERP";
    capitalize_string_remaps["etc"] = "ETC";
    capitalize_string_remaps["fbx"] = "FBX";
    capitalize_string_remaps["fft"] = "FFT";
    capitalize_string_remaps["fg"] = "FG";
    capitalize_string_remaps["fov"] = "FOV";
    capitalize_string_remaps["fps"] = "FPS";
    capitalize_string_remaps["fs"] = "FS";
    capitalize_string_remaps["fsr"] = "FSR";
    capitalize_string_remaps["fxaa"] = "FXAA";
    capitalize_string_remaps["gdscript"] = "GDScript";
    capitalize_string_remaps["ggx"] = "GGX";
    capitalize_string_remaps["gi"] = "GI";
    capitalize_string_remaps["glb"] = "GLB";
    capitalize_string_remaps["gles2"] = "GLES2";
    capitalize_string_remaps["gles3"] = "GLES3";
    capitalize_string_remaps["gpu"] = "GPU";
    capitalize_string_remaps["gui"] = "GUI";
    capitalize_string_remaps["guid"] = "GUID";
    capitalize_string_remaps["hdr"] = "HDR";
    capitalize_string_remaps["hidpi"] = "hiDPI";
    capitalize_string_remaps["hipass"] = "High-pass";
    capitalize_string_remaps["hseparation"] = "H Separation";
    capitalize_string_remaps["hsv"] = "HSV";
    capitalize_string_remaps["html"] = "HTML";
    capitalize_string_remaps["http"] = "HTTP";
    capitalize_string_remaps["id"] = "ID";
    capitalize_string_remaps["igd"] = "IGD";
    capitalize_string_remaps["ik"] = "IK";
    capitalize_string_remaps["image@2x"] = "Image @2x";
    capitalize_string_remaps["image@3x"] = "Image @3x";
    capitalize_string_remaps["ios"] = "iOS";
    capitalize_string_remaps["iod"] = "IOD";
    capitalize_string_remaps["ip"] = "IP";
    capitalize_string_remaps["ipad"] = "iPad";
    capitalize_string_remaps["iphone"] = "iPhone";
    capitalize_string_remaps["ipv6"] = "IPv6";
    capitalize_string_remaps["ir"] = "IR";
    capitalize_string_remaps["itunes"] = "iTunes";
    capitalize_string_remaps["jit"] = "JIT";
    capitalize_string_remaps["k1"] = "K1";
    capitalize_string_remaps["k2"] = "K2";
    capitalize_string_remaps["kb"] = "(KB)"; // Unit.
    capitalize_string_remaps["ldr"] = "LDR";
    capitalize_string_remaps["lod"] = "LOD";
    capitalize_string_remaps["lowpass"] = "Low-pass";
    capitalize_string_remaps["macos"] = "macOS";
    capitalize_string_remaps["mb"] = "(MB)"; // Unit.
    capitalize_string_remaps["mms"] = "MMS";
    capitalize_string_remaps["ms"] = "(ms)"; // Unit
    // Not used for now as AudioEffectReverb has a `msec` property.
    // capitalize_string_remaps["msec"] = "(msec)"; // Unit.
    capitalize_string_remaps["msaa"] = "MSAA";
    capitalize_string_remaps["nfc"] = "NFC";
    capitalize_string_remaps["normalmap"] = "Normal Map";
    capitalize_string_remaps["ofs"] = "Offset";
    capitalize_string_remaps["ok"] = "OK";
    capitalize_string_remaps["opengl"] = "OpenGL";
    capitalize_string_remaps["opentype"] = "OpenType";
    capitalize_string_remaps["openxr"] = "OpenXR";
    capitalize_string_remaps["pck"] = "PCK";
    capitalize_string_remaps["png"] = "PNG";
    capitalize_string_remaps["po2"] = "(Power of 2)"; // Unit.
    capitalize_string_remaps["pvs"] = "PVS";
    capitalize_string_remaps["pvrtc"] = "PVRTC";
    capitalize_string_remaps["rgb"] = "RGB";
    capitalize_string_remaps["rid"] = "RID";
    capitalize_string_remaps["rmb"] = "RMB";
    capitalize_string_remaps["rpc"] = "RPC";
    capitalize_string_remaps["s3tc"] = "S3TC";
    capitalize_string_remaps["sdf"] = "SDF";
    capitalize_string_remaps["sdfgi"] = "SDFGI";
    capitalize_string_remaps["sdk"] = "SDK";
    capitalize_string_remaps["sec"] = "(sec)"; // Unit.
    capitalize_string_remaps["selectedframe"] = "Selected Frame";
    capitalize_string_remaps["sms"] = "SMS";
    capitalize_string_remaps["srgb"] = "sRGB";
    capitalize_string_remaps["ssao"] = "SSAO";
    capitalize_string_remaps["ssh"] = "SSH";
    capitalize_string_remaps["ssil"] = "SSIL";
    capitalize_string_remaps["ssl"] = "SSL";
    capitalize_string_remaps["stderr"] = "stderr";
    capitalize_string_remaps["stdout"] = "stdout";
    capitalize_string_remaps["sv"] = "SV";
    capitalize_string_remaps["svg"] = "SVG";
    capitalize_string_remaps["tcp"] = "TCP";
    capitalize_string_remaps["ui"] = "UI";
    capitalize_string_remaps["url"] = "URL";
    capitalize_string_remaps["urls"] = "URLs";
    capitalize_string_remaps["us"] = "(µs)"; // Unit.
    capitalize_string_remaps["usb"] = "USB";
    capitalize_string_remaps["usec"] = "(µsec)"; // Unit.
    capitalize_string_remaps["uuid"] = "UUID";
    capitalize_string_remaps["uv"] = "UV";
    capitalize_string_remaps["uv1"] = "UV1";
    capitalize_string_remaps["uv2"] = "UV2";
    capitalize_string_remaps["uwp"] = "UWP";
    capitalize_string_remaps["vadjust"] = "V Adjust";
    capitalize_string_remaps["valign"] = "V Align";
    capitalize_string_remaps["vector2"] = "Vector2";
    capitalize_string_remaps["vpn"] = "VPN";
    capitalize_string_remaps["vram"] = "VRAM";
    capitalize_string_remaps["vseparation"] = "V Separation";
    capitalize_string_remaps["vsync"] = "V-Sync";
    capitalize_string_remaps["wap"] = "WAP";
    capitalize_string_remaps["webp"] = "WebP";
    capitalize_string_remaps["webrtc"] = "WebRTC";
    capitalize_string_remaps["websocket"] = "WebSocket";
    capitalize_string_remaps["wifi"] = "Wi-Fi";
    capitalize_string_remaps["x86"] = "x86";
    capitalize_string_remaps["xr"] = "XR";
    capitalize_string_remaps["xy"] = "XY";
    capitalize_string_remaps["xz"] = "XZ";
    capitalize_string_remaps["yz"] = "YZ";
}

// Capitalizes property path segments.
String _capitalize_name(StringView p_name)  {
    init_names();

    auto cached = capitalize_string_cache.find_as(p_name);
    if (cached != capitalize_string_cache.end()) {
        return cached->second;
    }
    Vector<StringView> source_parts = StringUtils::split(p_name, "_", false);
    FixedVector<String, 12, true> processed_parts;
    for (StringView part : source_parts) {
        auto remap = capitalize_string_remaps.find_as(part);
        if (remap != capitalize_string_remaps.end()) {
            processed_parts.emplace_back(remap->second);
        } else {
            processed_parts.emplace_back(StringUtils::capitalize(part));
        }
    }
    const String capitalized = String::joined(processed_parts, " ");

    capitalize_string_cache.emplace(String(p_name), capitalized);
    return capitalized;
}

}

EditorPropertyNameStyle EditorPropertyNameProcessor::get_default_inspector_style() {
    const EditorPropertyNameStyle style =
            EDITOR_GET_T<EditorPropertyNameStyle>("interface/inspector/default_property_name_style");
    if (style == EditorPropertyNameStyle::LOCALIZED && !is_localization_available()) {
        return EditorPropertyNameStyle::CAPITALIZED;
    }
    return style;
}

EditorPropertyNameStyle EditorPropertyNameProcessor::get_settings_style() {
    const bool translate = EDITOR_GET_T<bool>("interface/editor/localize_settings");
    return translate ? EditorPropertyNameStyle::LOCALIZED : EditorPropertyNameStyle::CAPITALIZED;
}

EditorPropertyNameStyle EditorPropertyNameProcessor::get_tooltip_style(EditorPropertyNameStyle p_style) {
    return p_style == EditorPropertyNameStyle::LOCALIZED ? EditorPropertyNameStyle::CAPITALIZED : EditorPropertyNameStyle::LOCALIZED;
}

bool EditorPropertyNameProcessor::is_localization_available() {
    return EDITOR_GET("interface/editor/editor_language").as<String>() != "en";
}

String EditorPropertyNameProcessor::process_name(StringView p_name, EditorPropertyNameStyle p_style) {
    switch (p_style) {
        case EditorPropertyNameStyle::RAW: {
            return String(p_name);
        }
        case EditorPropertyNameStyle::CAPITALIZED: {
            return _capitalize_name(p_name);
        }
        case EditorPropertyNameStyle::LOCALIZED: {
            return TTRGET(_capitalize_name(p_name)).asCString();
        }
    }
    ERR_FAIL_V_MSG(String(p_name), "Unexpected property name style.");
}
