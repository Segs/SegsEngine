#pragma once
#include "core/ustring.h"
#include "core/pool_vector.h"

template<class T>
String PoolVector<T>::join(String delimiter) {
    String rs = "";
    int s = size();
    auto r = read();
    for (int i = 0; i < s; i++) {
        rs += r[i] + delimiter;
    }
    rs.erase(rs.length() - delimiter.length(), delimiter.length());
    return rs;
}
