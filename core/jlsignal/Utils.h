#pragma once

/////////////
// I. Asserts

#include <cassert>

// To disable runtime asserts, either:
// 1. Define JL_DISABLE_ASSERT as a global compiler macro,
// 2. Comment out JL_ENABLE_ASSERT below
#define JL_ENABLE_ASSERT
#if defined( JL_ENABLE_ASSERT ) && ! defined ( JL_DISABLE_ASSERT ) && ! defined (NDEBUG)
#   define JL_CHECKED_CALL(x) { bool wasOk = x; assert( wasOk ); }
#   define JL_ASSERT( _Expr ) assert( _Expr )
#else
#   define JL_CHECKED_CALL(x) { x; }
#   define JL_ASSERT( _Expr )
#endif

//////////////////////////////
// II. Miscellaneous utilities

#define JL_UNUSED( _a ) (void)( _a )

namespace jl
{

template< typename _to, typename _from >
_to BruteForceCast( _from p )
{
    union
    {
        _from from;
        _to to;
    } conversion;
    conversion.from = p;
    return conversion.to;
}

} // namespace jl
