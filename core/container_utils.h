#pragma once
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
