#pragma once

#include "core/godot_export.h"

#include "core/jlsignal/Utils.h"
#include "core/jlsignal/DoublyLinkedList.h"

namespace jl
{

// Forward declarations
class SignalBase;
// Derive from this class to receive signals
class GODOT_EXPORT SignalObserver
{
    // Public interface
public:
    virtual ~SignalObserver();

    void DisconnectAllSignals();
    void DisconnectSignal(SignalBase *pSignal);

    SignalObserver() {}
private:
    friend class SignalBase;

    void OnSignalConnect(SignalBase *pSignal)
    {
#if defined( JL_ENABLE_ASSERT ) && ! defined ( JL_DISABLE_ASSERT ) && ! defined (NDEBUG)
        const bool bAdded =
#endif
        m_oSignals.Add( pSignal );
        JL_ASSERT( bAdded );
    }
    void OnSignalDisconnect(SignalBase *pSignal)
    {
      //  OnSignalDisconnectInternal( pSignal );
        for ( SignalList::iterator i = m_oSignals.begin(); i.isValid(); )
        {
            if ( *i == pSignal )
                m_oSignals.erase( i );
            else
                ++i;
        }
    }

    // Signal list
public:
    using SignalList = DoublyLinkedList<SignalBase*>;
    enum { eAllocationSize = sizeof(SignalList::Node) };

private:
    SignalList m_oSignals;
};

class GODOT_EXPORT SignalBase
{
public:
    virtual ~SignalBase() = default;

    // Interface for derived signal classes
protected:
    // Disallow instances of this class
    SignalBase() = default;

    // Called on any connection to the observer.
    void NotifyObserverConnect( SignalObserver* pObserver ) { pObserver->OnSignalConnect(this); }

    // Called when no more connections exist to the observer.
    void NotifyObserverDisconnect( SignalObserver* pObserver ) { pObserver->OnSignalDisconnect(this); }

    // Private interface (for SignalObserver)
private:
    friend class SignalObserver;
    virtual void OnObserverDisconnect(SignalObserver *pObserver) = 0;

};

} // namespace jl
