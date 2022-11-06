#ifdef TRACY_ENABLE
#include "thirdparty/tracy/Tracy.hpp"

#define SCOPE_PROFILE(name) ZoneScopedN(#name)
#define SCOPE_PROFILE_GPU(name) ZoneScoped
#define SCOPE_AUTONAMED ZoneScoped
#define PROFILER_STARTFRAME(name) FrameMarkStart(name)
#define PROFILER_ENDFRAME(name) FrameMarkEnd(name)
#define PROFILE_VALUE(name,value) TracyPlot( name, value )
#define PROFILE_VALUE_CFG(name,type) TracyPlotConfig( name, type )
//#define TRACE_MEMORY
#ifdef TRACE_MEMORY
#define TRACE_ALLOC(p,sz) TracyAlloc(p,sz)
#define TRACE_ALLOC_S(p,sz,depth) TracyAllocS(p,sz,depth)
#define TRACE_FREE(p) TracyFree(p)
#define TRACE_ALLOC_N(p,sz,n) TracyAllocN(p,sz,n)
#define TRACE_ALLOC_NS(p,sz,depth,n) TracyAllocNS(p,sz,depth,n)
#define TRACE_FREE_N(p,n) TracyFreeN(p,n)
#else
#define TRACE_ALLOC(p,sz)
#define TRACE_FREE(p)
#define TRACE_ALLOC_S(p,sz,depth)
#define TRACE_ALLOC_N(p,sz,n)
#define TRACE_FREE_N(p,n)
#define TRACE_ALLOC_NS(p,sz,depth,n)
#endif
#else
#define SCOPE_PROFILE(name)
#define SCOPE_PROFILE_GPU(name)
#define SCOPE_AUTONAMED
#define PROFILER_FLIP()
#define PROFILER_STARTFRAME(name)
#define PROFILER_ENDFRAME(name)
#define PROFILE_VALUE(name,value)
#define PROFILE_VALUE_CFG(name,type)
#define TRACE_ALLOC(p,sz)
#define TRACE_ALLOC_S(p,sz,depth)
#define TRACE_ALLOC_NS(p,sz,depth,n)
#define TRACE_FREE(p)
#define TRACE_ALLOC_N(p,sz,n)
#define TRACE_FREE_N(p,n)

#endif
