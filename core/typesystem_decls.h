#pragma once

#include <stdint.h>

namespace GodotTypeInfo {
enum Metadata {
    METADATA_NONE,
    METADATA_INT_IS_INT8,
    METADATA_INT_IS_INT16,
    METADATA_INT_IS_INT32,
    METADATA_INT_IS_INT64,
    METADATA_INT_IS_UINT8,
    METADATA_INT_IS_UINT16,
    METADATA_INT_IS_UINT32,
    METADATA_INT_IS_UINT64,
    METADATA_REAL_IS_FLOAT,
    METADATA_REAL_IS_DOUBLE,
    METADATA_STRING_VIEW,
    METADATA_STRING_NAME,

};
}

enum MethodFlags {

    METHOD_FLAG_NORMAL = 1,
    METHOD_FLAG_EDITOR = 2,
    METHOD_FLAG_NOSCRIPT = 4,
    METHOD_FLAG_CONST = 8,
    METHOD_FLAG_REVERSE = 16, // used for events
    METHOD_FLAG_VIRTUAL = 32,
    METHOD_FLAG_FROM_SCRIPT = 64,
    METHOD_FLAG_VARARG = 128,
    METHOD_FLAGS_DEFAULT = METHOD_FLAG_NORMAL,
};
#ifdef None
#undef None
#endif
enum class PropertyHint : int8_t {
    None=0, ///< no hint provided.
    Range, ///< hint_text = "min,max,step,slider; //slider is optional"
    ExpRange, ///< hint_text = "min,max,step", exponential edit
    Enum, ///< hint_text= "val1,val2,val3,etc"
    ExpEasing, /// exponential easing function (Math::ease) use "attenuation" hint string to revert (flip h), "full" to also include in/out. (ie: "attenuation,inout")
    Length, ///< hint_text= "length" (as integer)
    SpriteFrame, // FIXME: Obsolete: drop whenever we can break compat. Keeping now for GDNative compat.
    KeyAccel, ///< hint_text= "length" (as integer)
    Flags, ///< hint_text= "flag1,flag2,etc" (as bit flags)
    Layers2DRenderer,
    Layers2DPhysics,
    Layers3DRenderer,
    Layers3DPhysics,
    File, ///< a file path must be passed, hint_text (optionally) is a filter "*.png,*.wav,*.doc,"
    Dir, ///< a directory path must be passed
    GlobalFile, ///< a file path must be passed, hint_text (optionally) is a filter "*.png,*.wav,*.doc,"
    GlobalDir, ///< a directory path must be passed
    ResourceType, ///< a resource object type
    MultilineText, ///< used for string properties that can contain multiple lines
    PlaceholderText, ///< used to set a placeholder text for string properties
    ColorNoAlpha, ///< used for ignoring alpha component when editing a color
    ImageCompressLossy,
    ImageCompressLossless,
    ObjectID,
    TypeString, ///< a type string, the hint is the base type to choose
    NodePathToEditedNode, ///< so something else can provide this (used in scripts)
    MethodOfVariantType, ///< a method of a type
    MethodOfBaseType, ///< a method of a base type
    MethodOfInstance, ///< a method of an instance
    MethodOfScript, ///< a method of a script & base
    PropertyOfVariantType, ///< a property of a type
    PropertyOfBaseType, ///< a property of a base type
    PropertyOfInstance, ///< a property of an instance
    PropertyOfScript, ///< a property of a script & base
    ObjectTooBig, ///< object is too big to send
    NodePathValidTypes,
    SaveFile, ///< a file path must be passed, hint_text (optionally) is a filter "*.png,*.wav,*.doc,". This opens a save dialog
    Max
    // When updating PropertyHint, also sync the hardcoded list in VisualScriptEditorVariableEdit
};

enum PropertyUsageFlags {

    PROPERTY_USAGE_STORAGE = 1,
    PROPERTY_USAGE_EDITOR = 2,
    PROPERTY_USAGE_NETWORK = 4,
    PROPERTY_USAGE_EDITOR_HELPER = 8,
    PROPERTY_USAGE_CHECKABLE = 16, //used for editing global variables
    PROPERTY_USAGE_CHECKED = 32, //used for editing global variables
    PROPERTY_USAGE_INTERNATIONALIZED = 64, //hint for internationalized strings
    PROPERTY_USAGE_GROUP = 128, //used for grouping props in the editor
    PROPERTY_USAGE_CATEGORY = 256,
    // FIXME: Drop in 4.0, possibly reorder other flags?
    // Those below are deprecated thanks to ClassDB's now class value cache
    //PROPERTY_USAGE_STORE_IF_NONZERO = 512, //only store if nonzero
    //PROPERTY_USAGE_STORE_IF_NONONE = 1024, //only store if false
    PROPERTY_USAGE_NO_INSTANCE_STATE = 2048,
    PROPERTY_USAGE_RESTART_IF_CHANGED = 4096,
    PROPERTY_USAGE_SCRIPT_VARIABLE = 8192,
    PROPERTY_USAGE_STORE_IF_NULL = 16384,
    PROPERTY_USAGE_ANIMATE_AS_TRIGGER = 32768,
    PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED = 65536,
    PROPERTY_USAGE_SCRIPT_DEFAULT_VALUE = 1 << 17,
    PROPERTY_USAGE_CLASS_IS_ENUM = 1 << 18,
    PROPERTY_USAGE_NIL_IS_VARIANT = 1 << 19,
    PROPERTY_USAGE_INTERNAL = 1 << 20,
    PROPERTY_USAGE_DO_NOT_SHARE_ON_DUPLICATE = 1 << 21, // If the object is duplicated also this property will be duplicated
    PROPERTY_USAGE_HIGH_END_GFX = 1 << 22,
    PROPERTY_USAGE_NODE_PATH_FROM_SCENE_ROOT = 1 << 23,
    PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT = 1 << 24,
    PROPERTY_USAGE_KEYING_INCREMENTS = 1 << 25, // Used in inspector to increment property when keyed in animation player

    PROPERTY_USAGE_DEFAULT = PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_NETWORK,
    PROPERTY_USAGE_DEFAULT_INTL = PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_NETWORK | PROPERTY_USAGE_INTERNATIONALIZED,
    PROPERTY_USAGE_NOEDITOR = PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_NETWORK,
};

enum class TypePassBy : int8_t {
    Value,
    Reference,
    Pointer,
};

/* This is a skeleton version of actual property info, used to reduce the include hell and allow constexpr construction.*/
struct RawPropertyInfo {
    const char *name=nullptr;
    const char *hint_string=nullptr;
    const char *class_name=nullptr; // for classes
    int8_t type = 0;
    PropertyHint hint = PropertyHint::None;
    uint32_t usage = PROPERTY_USAGE_DEFAULT;

};
