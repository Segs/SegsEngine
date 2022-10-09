#pragma once

// This file contains no-op macros used by the reflection compilaton system to properly process c++ things

// Defines a namespace, all definitions following will be put into this namespace.
// This is active until matching SE_END or end of currently processed file.
#define SE_NAMESPACE(x)
#define SE_CONSTANT(x, ...)
#define SE_ENUM(x, ...)

// Must be placed directly after `class X {`
#define SE_CLASS(...) \
    //static void _init_reflection();

#define SE_OPAQUE_TYPE(x)
/**
    similar syntax to Q_PROPERTY
    (type name
        READ getFunction [WRITE setFunction]
       [RESET resetFunction]
       [NOTIFY notifySignal]
       [USAGE STORAGE|...] // any of PropertyUsageFlags without the leading PROPERTY_USAGE_
       [META_FUNC metaFunc]
       [GROUP group_name]
    )
    if type is an array, denoted by adding [OPTIONAL_MAX_SIZE] to it's name, the type must follow array interface:
        void NAME_resize(int);
        int NAME_size() const;
        void NAME_push_back(T);


    META_FUNC provides the capability of querying things like value ranges, icons, hints etc.
              metaFunc prototype is Variant metaFunc(PropertyMetaData,Variant);
    GROUP group_name is used to group multiple properties under a single group, and it's only used to locate the
              actual property under which the group's entries should be put.

)
*/

#define SE_PROPERTY(...)
#define SE_END()

#define SE_INVOCABLE
#define SE_SIGNAL
//TODO: valid until next access specifier (public:/private:/protected:) or end of class.
//#define SE_SIGNALS
