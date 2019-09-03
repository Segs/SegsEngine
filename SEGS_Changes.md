1. Move utility methods from String class to separate namespaces (StringUtils,PathUtils) - those namespaces are interoperability helpers, since the underlying string type might change, but StringUtils will just implement required functionality over a new string.
2. Temporarily replaced String and CharString with QString/QByteArray, likely will go away when we switch over to eastl::string/eastl::string<char16_t>

