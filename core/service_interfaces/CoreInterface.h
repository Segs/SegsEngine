#pragma once

#include <stdint.h>
#include "core/godot_export.h"

class FileAccess;
class String;
/** This interface is used by the infrastructure plugins to interact with the engine core
 * Main usages revolve around error/progress reporting and image memory allocations
*/
class GODOT_EXPORT CoreInterface 
{
public:
	virtual FileAccess * wrapMemoryAsFileAccess(const uint8_t *data,int sz) = 0;
	virtual void  releaseFileAccess(FileAccess *) = 0;

	virtual void reportError(const String &msg,const char *retval,const char *funcstr, const char *file,int line) = 0;
	virtual void clearLastError() = 0;

};
GODOT_EXPORT CoreInterface *getCoreInterface();

#define PLUG_FAIL_V_MSG(m_value, m_msg)															\
	{																							\
		getCoreInterface()->reportError(m_msg,__STR(m_value),FUNCTION_STR, __FILE__, __LINE__); \
		return m_value;                                                                         \
	}
#define PLUG_FAIL_V(m_value)                                                                                       \
	{                                                                                                             \
		getCoreInterface()->reportError(String(),__STR(m_value),FUNCTION_STR, __FILE__, __LINE__); \
		return m_value;                                                                                           \
	}
#define PLUG_FAIL_COND_V(m_cond, m_retval)                                                                                            \
	{                                                                                                                                \
		if (unlikely(m_cond)) {                                                                                                      \
			_err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Condition ' " _STR(m_cond) " ' is true. returned: " _STR(m_retval)); \
			return m_retval;                                                                                                         \
		}                                                                                                                            \
		getCoreInterface()->clearLastError();                                                                                        \
	}

#define PLUG_FAIL_COND_V_MSG(m_cond, m_retval, m_msg)                                                                                \
	{                                                                                                                                \
		if (unlikely(m_cond)) {                                                                                                      \
			ERR_EXPLAIN(m_msg);                                                                                                      \
			_err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Condition ' " _STR(m_cond) " ' is true. returned: " _STR(m_retval)); \
			return m_retval;                                                                                                         \
		}                                                                                                                            \
		getCoreInterface()->clearLastError();                                                                                        \
	}
