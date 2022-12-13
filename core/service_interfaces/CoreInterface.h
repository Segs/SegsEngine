#pragma once

#include "core/godot_export.h"
#include "core/forward_decls.h"
#include <cstdint>

class FileAccess;
using UIString = class QString;

/** This interface is used by the infrastructure plugins to interact with the engine core
 * For now main usages revolve around error/progress reporting and image memory allocations
*/
class GODOT_EXPORT CoreInterface
{
public:
    virtual FileAccess * wrapMemoryAsFileAccess(const uint8_t *data,int sz) = 0;
    virtual void  releaseFileAccess(FileAccess *) = 0;

    virtual void reportError(StringView msg,const char *retval,const char *funcstr, const char *file,int line) = 0;
    virtual void clearLastError() = 0;
    virtual void fillVersion(uint32_t &major,uint32_t &minor,uint32_t &patch) = 0;

};
GODOT_EXPORT CoreInterface *getCoreInterface();

#define PLUG_FAIL_V_MSG(m_value, m_msg)                                                                                                              \
    {                                                                                                                                                \
        getCoreInterface()->reportError(m_msg, __STR(m_value), FUNCTION_STR, __FILE__, __LINE__);                                                    \
        return m_value;                                                                                                                              \
    }
#define PLUG_FAIL_V(m_value)                                                                                       \
    {                                                                                                             \
    getCoreInterface()->reportError({},__STR(m_value),FUNCTION_STR, __FILE__, __LINE__); \
        return m_value;                                                                                           \
    }
#define PLUG_FAIL_COND_V(m_cond, m_retval)                                                                                            \
    {                                                                                                                                \
        if (unlikely(m_cond)) {                                                                                                      \
            getCoreInterface()->reportError("Condition ' " _STR(m_cond) " ' is true ",_STR(m_retval),FUNCTION_STR, __FILE__, __LINE__); \
            return m_retval;                                                                                                         \
        }                                                                                                                            \
        getCoreInterface()->clearLastError();                                                                                        \
    }

#define PLUG_FAIL_COND_V_MSG(m_cond, m_retval, m_msg)                                                                                \
    {                                                                                                                                \
        if (unlikely(m_cond)) {                                                                                                      \
            ERR_EXPLAIN(m_msg);                                                                                                      \
            getCoreInterface()->reportError("Condition ' " _STR(m_cond) " ' is true.",_STR(m_retval),FUNCTION_STR, __FILE__, __LINE__); \
            return m_retval;                                                                                                         \
        }                                                                                                                            \
        getCoreInterface()->clearLastError();                                                                                        \
    }
