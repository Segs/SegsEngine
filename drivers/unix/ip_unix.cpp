/*************************************************************************/
/*  ip_unix.cpp                                                          */
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

#include "core/class_db.h"
#include "ip_unix.h"

#if defined(UNIX_ENABLED) || defined(WINDOWS_ENABLED)
#include "core/property_info.h"
#include "core/string_utils.h"
#include "core/string_utils.inl"

#include <cstring>

#ifdef WINDOWS_ENABLED
#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else // UNIX
#include <netdb.h>
#ifdef __FreeBSD__
#include <sys/types.h>
#endif
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#ifdef __FreeBSD__
#include <netinet/in.h>
#endif
#include <net/if.h> // Order is important on OpenBSD, leave as last
#endif

static IP_Address _sockaddr2ip(struct sockaddr *p_addr) {
    IP_Address ip;

    if (p_addr->sa_family == AF_INET) {
        struct sockaddr_in *addr = (struct sockaddr_in *)p_addr;
        ip.set_ipv4((uint8_t *)&(addr->sin_addr));
    } else if (p_addr->sa_family == AF_INET6) {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)p_addr;
        ip.set_ipv6(addr6->sin6_addr.s6_addr);
    };

    return ip;
};

IMPL_GDCLASS(IP_Unix)

void IP_Unix::_resolve_hostname(Vector<IP_Address> &r_addresses,StringView p_hostname, Type p_type) {
    struct addrinfo hints;
    struct addrinfo *result;

    memset(&hints, 0, sizeof(struct addrinfo));
    if (p_type == TYPE_IPV4) {
        hints.ai_family = AF_INET;
    } else if (p_type == TYPE_IPV6) {
        hints.ai_family = AF_INET6;
        hints.ai_flags = 0;
    } else {
        hints.ai_family = AF_UNSPEC;
        hints.ai_flags = AI_ADDRCONFIG;
    }
    hints.ai_flags &= ~AI_NUMERICHOST;
    String sd(p_hostname);
    int s = getaddrinfo(sd.data(), nullptr, &hints, &result);
    if (s != 0) {
        ERR_PRINT("getaddrinfo failed! Cannot resolve hostname.");
        return;
    }

    if (result == nullptr || result->ai_addr == nullptr) {
        ERR_PRINT("Invalid response from getaddrinfo");
        if (result) {
            freeaddrinfo(result);
        }
        return;
    }
    struct addrinfo *next = result;

    do {
        if (next->ai_addr == nullptr) {
            next = next->ai_next;
            continue;
        }
        IP_Address ip = _sockaddr2ip(next->ai_addr);
        if (ip.is_valid() && !r_addresses.contains(ip)) {
            r_addresses.emplace_back(ip);
        }
        next = next->ai_next;
    } while (next);

    freeaddrinfo(result);

}

#if defined(WINDOWS_ENABLED)


void IP_Unix::get_local_interfaces(Map<String, Interface_Info> *r_interfaces) const {
    ULONG buf_size = 1024;
    IP_ADAPTER_ADDRESSES *addrs;

    while (true) {
        addrs = (IP_ADAPTER_ADDRESSES *)memalloc(buf_size);
        int err = GetAdaptersAddresses(AF_UNSPEC,
                GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER |
                        GAA_FLAG_SKIP_FRIENDLY_NAME,
                nullptr, addrs, &buf_size);
        if (err == NO_ERROR) {
            break;
        };
        memfree(addrs);
        if (err == ERROR_BUFFER_OVERFLOW) {
            continue; // will go back and alloc the right size
        };

        ERR_FAIL_MSG("Call to GetAdaptersAddresses failed with error " + itos(err) + ".");
    };

    IP_ADAPTER_ADDRESSES *adapter = addrs;

    while (adapter != nullptr) {
        Interface_Info info;
        info.name = adapter->AdapterName;
        info.name_friendly = StringUtils::to_utf8(StringUtils::from_wchar(adapter->FriendlyName));
        info.index = adapter->IfIndex;

        IP_ADAPTER_UNICAST_ADDRESS *address = adapter->FirstUnicastAddress;
        while (address != nullptr) {
            int family = address->Address.lpSockaddr->sa_family;
            if (family != AF_INET && family != AF_INET6)
                continue;
            info.ip_addresses.push_front(_sockaddr2ip(address->Address.lpSockaddr));
            address = address->Next;
        }
        adapter = adapter->Next;
        // Only add interface if it has at least one IP
        if (info.ip_addresses.size() > 0)
            r_interfaces->emplace(info.name, info);
    };

    memfree(addrs);
};


#else // UNIX

void IP_Unix::get_local_interfaces(Map<String, Interface_Info> *r_interfaces) const {
    struct ifaddrs *ifAddrStruct = nullptr;
    struct ifaddrs *ifa = nullptr;
    int family;

    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }

        family = ifa->ifa_addr->sa_family;

        if (family != AF_INET && family != AF_INET6) {
            continue;
        }

        Map<String, Interface_Info>::iterator E = r_interfaces->find(ifa->ifa_name);
        if (E == r_interfaces->end()) {
            Interface_Info info;
            info.name = ifa->ifa_name;
            info.name_friendly = ifa->ifa_name;
            info.index = if_nametoindex(ifa->ifa_name);
            auto insert_res = r_interfaces->emplace(ifa->ifa_name, info);
            E = insert_res.first;
            ERR_CONTINUE(insert_res.second == false);
        }

        Interface_Info &info = E->second;
        info.ip_addresses.push_front(_sockaddr2ip(ifa->ifa_addr));
    }

    if (ifAddrStruct != nullptr) {
        freeifaddrs(ifAddrStruct);
    }
}
#endif

void IP_Unix::make_default() {
    _create = _create_unix;
}

IP *IP_Unix::_create_unix() {
    IP_Unix::initialize_class();
    return memnew(IP_Unix);
}

IP_Unix::IP_Unix() = default;

#endif
