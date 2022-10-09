#pragma once

#include "core/godot_export.h"

#include "core/os/memory.h"

#include <stdint.h>
/**
 * @brief FrameAllocator class performs very fast allocations but can only free all of its memory at once.
 * Very useful for short-lived allocations ( per-frame )
 *
 * @note This class methods are not thread safe except for
 */
class GODOT_EXPORT FrameAllocator {
    struct Block {
        Block(uint32_t size) : m_size(size) {}
        /**
         \brief Allocates an \a amount of memory within the block. Caller is responsible for verifying that block has enough empty space.
         \param amount
         \return pointer to the requested memory
        */
        uint8_t* alloc(uint32_t amount)
        {
            uint8_t *res = m_data + m_free_ptr;
            m_free_ptr += amount;
            return res;
        }
        /**
         * \brief Releases all allocations within a block but doesn't actually free the underlying memory.
         */
        void reset() { m_free_ptr = 0; }

        uint8_t *m_data = nullptr;
        uint32_t m_free_ptr = 0; //!< Mark the first free location in m_data buffer.
        uint32_t m_size;
    };
public:
    FrameAllocator(uint32_t blockSize = 1024 * 1024);
    ~FrameAllocator();
private:
    uint32_t m_block_size;

};
