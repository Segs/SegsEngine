#define RMT_USE_OPENGL 1

#ifdef TRACY_ENABLE
#include "thirdparty/tracy/Tracy.hpp"

#define SCOPE_PROFILE(name) ZoneScopedN(#name)
#define SCOPE_PROFILE_GPU(name) ZoneScoped
#define SCOPE_AUTONAMED ZoneScoped
#define PROFILER_STARTFRAME(name) FrameMarkStart(name)
#define PROFILER_ENDFRAME(name) FrameMarkEnd(name)
//#define TRACE_MEMORY
#ifdef TRACE_MEMORY
#define TRACE_ALLOC(p,sz) TracyAlloc(p,sz)
#define TRACE_FREE(p) TracyFree(p)
#else
#define TRACE_ALLOC(p,sz)
#define TRACE_FREE(p)
#endif
#else
#define SCOPE_PROFILE(name)
#define SCOPE_PROFILE_GPU(name)
#define SCOPE_AUTONAMED
#define PROFILER_FLIP()
#define PROFILER_STARTFRAME(name)
#define PROFILER_ENDFRAME(name)
#define TRACE_ALLOC(p,sz)
#define TRACE_FREE(p)

#endif
