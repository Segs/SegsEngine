#pragma once
#include "core/pool_vector.h"


namespace ContainerUtils
{
template<typename RAContainer,typename T>
bool contains(const RAContainer &c,const T &v)
{
    for(const auto &entry : c)
        if(T(entry)==v)
            return true;
    return false;
}
} // end of ContainerUtils namespace
