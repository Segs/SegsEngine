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
    StringUtils::erase(rs,rs.length() - delimiter.length(), delimiter.length());
    return rs;
}
namespace ContainerUtils
{
template<typename RAContainer,typename T>
bool contains(const RAContainer &c,const T &v)
{
    for(const T &entry : c)
        if(entry==v)
            return true;
    return false;
}
} // end of ContainerUtils namespace
