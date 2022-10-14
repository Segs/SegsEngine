/*************************************************************************/
/*  ip.cpp                                                               */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "ip.h"

#include "core/hash_map.h"
#include "core/dictionary.h"
#include "core/method_bind.h"
#include "core/method_enum_caster.h"
#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"

IMPL_GDCLASS(IP)

VARIANT_ENUM_CAST(IP::ResolverStatus);
VARIANT_ENUM_CAST(IP::Type);

namespace {

IP_Address first_valid_address(const Vector<IP_Address> &addresses) {
    for(auto &entry: addresses) {
        if(entry.is_valid())
            return entry;
    }
    return {};
}
}
/************* RESOLVER ******************/

struct _IP_ResolverPrivate {

    struct QueueItem {
        SafeNumeric<IP::ResolverStatus> status;
        Vector<IP_Address> response;
        String hostname;
        IP::Type type;

        void clear() {
            status.set(IP::RESOLVER_STATUS_NONE);
            response.clear();
            type = IP::TYPE_NONE;
            hostname = "";
        }

        QueueItem() {
            clear();
        }
    };

    QueueItem queue[IP::RESOLVER_MAX_QUERIES];

    IP::ResolverID find_empty_id() const {

        for (int i = 0; i < IP::RESOLVER_MAX_QUERIES; i++) {
            if (queue[i].status.get() == IP::RESOLVER_STATUS_NONE) {
                return i;
            }
        }
        return IP::RESOLVER_INVALID_ID;
    }

    Mutex mutex;
    Semaphore sem;

    Thread thread;
    //Semaphore* semaphore;
    bool thread_abort;

    void resolve_queues() {

        for (int i = 0; i < IP::RESOLVER_MAX_QUERIES; i++) {

            if (queue[i].status.get() != IP::RESOLVER_STATUS_WAITING)
                continue;

            String hostname;
            Vector<IP_Address> response;
            IP::Type type;
            {
                MutexGuard guard(mutex);
                hostname = queue[i].hostname;
                type = queue[i].type;
            }

            // We should not lock while resolving the hostname,
            // only when modifying the queue.
            IP::get_singleton()->_resolve_hostname(response,hostname, type);

            MutexLock lock(mutex);
            // Could have been completed by another function, or deleted.
            if (queue[i].status.get() != IP::RESOLVER_STATUS_WAITING) {
                continue;
            }
            // We might be overriding another result, but we don't care as long as the result is valid.
            if (!response.empty()) {
                String key = get_cache_key(hostname, type);
                cache[key] = response;
            }
            queue[i].response = response;
            queue[i].status.set(response.empty() ? IP::RESOLVER_STATUS_ERROR : IP::RESOLVER_STATUS_DONE);
        }
    }

    static void _thread_function(void *self) {

        _IP_ResolverPrivate *ipr = (_IP_ResolverPrivate *)self;

        while (!ipr->thread_abort) {

            ipr->sem.wait();

            ipr->resolve_queues();
        }
    }

    HashMap<String, Vector<IP_Address> > cache;

    static String get_cache_key(StringView p_hostname, IP::Type p_type) {
        return ::to_string(p_type) + p_hostname;
    }
};

IP_Address IP::resolve_hostname(StringView p_hostname, IP::Type p_type) {
    Vector<IP_Address> res;
    String key = _IP_ResolverPrivate::get_cache_key(p_hostname, p_type);

    resolver->mutex.lock();
    if (resolver->cache.contains(key)) {
        res = resolver->cache[key];
    } else {
        // This should be run unlocked so the resolver thread can keep
        // resolving other requests.
        resolver->mutex.unlock();
        _resolve_hostname(res, p_hostname, p_type);
        resolver->mutex.lock();
        // We might be overriding another result, but we don't care (they are the
        // same hostname).
        resolver->cache[key] = res;
    }
    resolver->mutex.unlock();
    return first_valid_address(res);
}

Array IP::resolve_hostname_addresses(StringView p_hostname, Type p_type) {
    Vector<IP_Address> res;
    String key = _IP_ResolverPrivate::get_cache_key(p_hostname, p_type);

    resolver->mutex.lock();
    if (resolver->cache.contains(key)) {
        res = resolver->cache[key];
    } else {
        // This should be run unlocked so the resolver thread can keep resolving
        // other requests.
        resolver->mutex.unlock();
        _resolve_hostname(res, p_hostname, p_type);
        resolver->mutex.lock();
        // We might be overriding another result, but we don't care as long as the result is valid.
        if (!res.empty()) {
            resolver->cache[key] = res;
        }
    }
    resolver->mutex.unlock();

    Array result;
    for (int i = 0; i < res.size(); ++i) {
        if (res[i].is_valid()) {
            result.push_back(String(res[i]));
        }
    }
    return result;
}

IP::ResolverID IP::resolve_hostname_queue_item(const String &p_hostname, IP::Type p_type) {

    MutexLock guard(resolver->mutex);

    ResolverID id = resolver->find_empty_id();

    if (id == RESOLVER_INVALID_ID) {
        WARN_PRINT("Out of resolver queries");
        return id;
    }

    String key = _IP_ResolverPrivate::get_cache_key(p_hostname, p_type);
    resolver->queue[id].hostname = p_hostname;
    resolver->queue[id].type = p_type;
    if (resolver->cache.contains(key)) {
        resolver->queue[id].response = resolver->cache[key];
        resolver->queue[id].status.set(IP::RESOLVER_STATUS_DONE);
    } else {
        resolver->queue[id].response = {};
        resolver->queue[id].status.set(IP::RESOLVER_STATUS_WAITING);
        if (resolver->thread.is_started())
            resolver->sem.post();
        else
            resolver->resolve_queues();
    }

    return id;
}

IP::ResolverStatus IP::get_resolve_item_status(ResolverID p_id) const {

    ERR_FAIL_INDEX_V(p_id, IP::RESOLVER_MAX_QUERIES, IP::RESOLVER_STATUS_NONE);

    IP::ResolverStatus res = resolver->queue[p_id].status.get();
    if (res == IP::RESOLVER_STATUS_NONE) {
        ERR_PRINT("Condition status == IP::RESOLVER_STATUS_NONE");
        return IP::RESOLVER_STATUS_NONE;
    }
    return res;
}

IP_Address IP::get_resolve_item_address(ResolverID p_id) const {

    ERR_FAIL_INDEX_V(p_id, IP::RESOLVER_MAX_QUERIES, IP_Address());

    MutexLock guard(resolver->mutex);

    if (resolver->queue[p_id].status.get() != IP::RESOLVER_STATUS_DONE) {
        ERR_PRINT("Resolve of '" + resolver->queue[p_id].hostname + "'' didn't complete yet.");
        return IP_Address();
    }
    return first_valid_address(resolver->queue[p_id].response);
    }

Array IP::get_resolve_item_addresses(ResolverID p_id) const {
    ERR_FAIL_INDEX_V(p_id, IP::RESOLVER_MAX_QUERIES, Array());

    MutexLock lock(resolver->mutex);

    if (resolver->queue[p_id].status.get() != IP::RESOLVER_STATUS_DONE) {
        ERR_PRINT("Resolve of '" + resolver->queue[p_id].hostname + "'' didn't complete yet.");
        return Array();
}
    Vector<IP_Address> res = resolver->queue[p_id].response;

    Array result;
    for (int i = 0; i < res.size(); ++i) {
        if (res[i].is_valid()) {
            result.push_back(String(res[i]));
        }
    }
    return result;
}

void IP::erase_resolve_item(ResolverID p_id) {

    ERR_FAIL_INDEX(p_id, IP::RESOLVER_MAX_QUERIES);

    resolver->queue[p_id].status.set(IP::RESOLVER_STATUS_NONE);
}

void IP::clear_cache(const String &p_hostname) {

    MutexLock guard(resolver->mutex);

    if (p_hostname.empty()) {
        resolver->cache.clear();
    } else {
        resolver->cache.erase(_IP_ResolverPrivate::get_cache_key(p_hostname, IP::TYPE_NONE));
        resolver->cache.erase(_IP_ResolverPrivate::get_cache_key(p_hostname, IP::TYPE_IPV4));
        resolver->cache.erase(_IP_ResolverPrivate::get_cache_key(p_hostname, IP::TYPE_IPV6));
        resolver->cache.erase(_IP_ResolverPrivate::get_cache_key(p_hostname, IP::TYPE_ANY));
    }
}

Array IP::_get_local_addresses() const {

    Array addresses;
    Vector<IP_Address> ip_addresses;
    get_local_addresses(&ip_addresses);
    for (const IP_Address &E : ip_addresses) {
        addresses.push_back(Variant(E));
    }

    return addresses;
}

Array IP::_get_local_interfaces() const {

    Array results;
    Map<String, Interface_Info> interfaces;
    get_local_interfaces(&interfaces);
    for (eastl::pair<const String,Interface_Info> &E : interfaces) {
        Interface_Info &c(E.second);
        Dictionary rc;
        rc["name"] = Variant(c.name);
        rc["friendly"] = Variant(c.name_friendly);
        rc["index"] = c.index;

        Array ips;
        for (const IP_Address &F : c.ip_addresses) {
            ips.push_front(Variant(F));
        }
        rc["addresses"] = ips;

        results.push_front(rc);
    }

    return results;
}

void IP::get_local_addresses(Vector<IP_Address> *r_addresses) const {

    Map<String, Interface_Info> interfaces;
    get_local_interfaces(&interfaces);
    for (eastl::pair<const String,Interface_Info> &E : interfaces) {
        for (const IP_Address &F : E.second.ip_addresses) {
            r_addresses->push_front(F);
        }
    }
}

void IP::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("resolve_hostname", {"host", "ip_type"}), &IP::resolve_hostname, {DEFVAL(IP::TYPE_ANY)});
    MethodBinder::bind_method(D_METHOD("resolve_hostname_addresses", {"host", "ip_type"}), &IP::resolve_hostname_addresses, {DEFVAL(IP::TYPE_ANY)});
    MethodBinder::bind_method(D_METHOD("resolve_hostname_queue_item", {"host", "ip_type"}), &IP::resolve_hostname_queue_item, {DEFVAL(IP::TYPE_ANY)});
    BIND_METHOD(IP,get_resolve_item_status);
    BIND_METHOD(IP,get_resolve_item_address);
    BIND_METHOD(IP,get_resolve_item_addresses);
    BIND_METHOD(IP,erase_resolve_item);
    MethodBinder::bind_method(D_METHOD("get_local_addresses"), &IP::_get_local_addresses);
    MethodBinder::bind_method(D_METHOD("get_local_interfaces"), &IP::_get_local_interfaces);
    MethodBinder::bind_method(D_METHOD("clear_cache", {"hostname"}), &IP::clear_cache, {DEFVAL("")});

    BIND_ENUM_CONSTANT(RESOLVER_STATUS_NONE);
    BIND_ENUM_CONSTANT(RESOLVER_STATUS_WAITING);
    BIND_ENUM_CONSTANT(RESOLVER_STATUS_DONE);
    BIND_ENUM_CONSTANT(RESOLVER_STATUS_ERROR);

    BIND_CONSTANT(RESOLVER_MAX_QUERIES)
    BIND_CONSTANT(RESOLVER_INVALID_ID)

    BIND_ENUM_CONSTANT(TYPE_NONE);
    BIND_ENUM_CONSTANT(TYPE_IPV4);
    BIND_ENUM_CONSTANT(TYPE_IPV6);
    BIND_ENUM_CONSTANT(TYPE_ANY);
}

IP *IP::singleton = nullptr;

IP *IP::get_singleton() {

    return singleton;
}

IP *(*IP::_create)() = nullptr;

IP *IP::create() {

    ERR_FAIL_COND_V_MSG(singleton, nullptr, "IP singleton already exists.");
    ERR_FAIL_COND_V(!_create, nullptr);
    return _create();
}

IP::IP() {

    singleton = this;
    resolver = memnew(_IP_ResolverPrivate);

    resolver->thread_abort = false;
    resolver->thread.start(_IP_ResolverPrivate::_thread_function, resolver);
}

IP::~IP() {
    resolver->thread_abort = true;
    resolver->sem.post();
    resolver->thread.wait_to_finish();
    memdelete(resolver);
}
