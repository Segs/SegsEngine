#include "SignalBase.h"
#include "Signal.h"
#include "SignalDefinitions.h"
#include "ObjectPool.h"


#include "core/os/memory.h"
#include "core/deque.h"

namespace jl {
jl::SignalObserver::~SignalObserver() {
    DisconnectAllSignals();
}

void jl::SignalObserver::DisconnectSignal(SignalBase *pSignal) {
    for (SignalBase *sig : m_oSignals) {
        if (sig == pSignal) {
            JL_SIGNAL_LOG("Observer %p disconnecting signal %p", this, pSignal);
            sig->OnObserverDisconnect(this);
            break;
        }
    }
}

void jl::SignalObserver::DisconnectAllSignals() {
    JL_SIGNAL_LOG("Observer %p disconnecting all signals\n", this);

    for (SignalBase *sig : m_oSignals)
        sig->OnObserverDisconnect(this);

    m_oSignals.clear();
}

} // namespace jl
