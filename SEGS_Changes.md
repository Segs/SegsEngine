1. Move utility methods from String class to separate namespaces (StringUtils,PathUtils) - those namespaces are interoperability helpers, since the underlying string type might change, but StringUtils will just implement required functionality over a new string.
2. Replaced String with eastl::string and used QString as an UI string class
3. Replaced Set/Map with eastl classes
4. Replaced most CoW vector usages with eastl::vector


