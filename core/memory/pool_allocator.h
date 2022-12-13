#pragma once

/**
 * A memory allocator that allocates elements of the same size. Allows for fairly quick allocations and deallocations.
 *
 * @tparam ElemSize         Size of a single element in the pool. This will be the exact allocation size. 4 byte minimum.
 * @tparam ElemsPerBlock    Determines how much space to reserve for elements. This determines the initial size of the
 *                            pool, and the additional size the pool will be expanded by every time the number of elements
 *                            goes over the available storage limit.
 * @tparam Alignment        Memory alignment of each allocated element. Note that alignments that are larger than
 *                              element size, or aren't a multiplier of element size will introduce additionally padding
 *                              for each element, and therefore require more internal memory.
 * @tparam Lock  If true the pool allocator will be made thread safe (at the cost of performance).
 */
template <int ElemSize, int ElemsPerBlock = 512, int Alignment = 4, bool Lock = false>
class PoolAlloc {

};


template <typename T>
class TypedPool : PoolAlloc<sizeof(T)>
{};
