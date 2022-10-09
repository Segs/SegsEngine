#ifndef ENTT_SIGNAL_FWD_HPP
#define ENTT_SIGNAL_FWD_HPP

#include "EASTL/memory.h"

namespace entt {

template<typename>
class delegate;

class dispatcher;

template<typename>
class emitter;

class connection;

struct scoped_connection;

template<typename>
class sink;

template<typename Type, typename = eastl::allocator>
class sigh;

} // namespace entt

#endif
