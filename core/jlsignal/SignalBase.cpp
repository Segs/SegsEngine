#include "SignalBase.h"

#include "Signal.h"

#include "SignalDefinitions.h"
using namespace jl;
namespace
{
ScopedAllocator* s_pCommonAllocator = nullptr;
}

template class EXPORT_TEMPLATE_DECLARE(GODOT_EXPORT) jl::Signal<>;

jl::SignalObserver::~SignalObserver()
{
    DisconnectAllSignals();
}

void jl::SignalObserver::DisconnectSignal( SignalBase* pSignal )
{
    for ( SignalBase* sig : m_oSignals)
    {
        if ( sig == pSignal )
        {
            JL_SIGNAL_LOG( "Observer %p disconnecting signal %p", this, pSignal );
            sig->OnObserverDisconnect( this );
            break;
        }
    }
}

void jl::SignalObserver::DisconnectAllSignals()
{
    JL_SIGNAL_LOG( "Observer %p disconnecting all signals\n", this );

    for ( SignalBase* sig : m_oSignals )
        sig->OnObserverDisconnect( this );

    m_oSignals.clear();
}
jl::ScopedAllocator *jl::defaultAllocator()
{
    return s_pCommonAllocator;
}

void jl::SignalBase::SetCommonConnectionAllocator(jl::ScopedAllocator *pAllocator)
{
    s_pCommonAllocator = pAllocator;
}
