/*************************************************************************/
/*  broad_phase_2d_basic.cpp                                             */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "broad_phase_2d_basic.h"
/*
.LC0:
        .string "(reinterpret_cast<size_t>(result)& ~(alignment - 1)) == reinterpret_cast<size_t>(result)"
.LC1:
        .string "the behaviour of eastl::allocators that return nullptr is not defined."
BroadPhase2DBasic::update():
        push    r15
        lea     rax, 16[rdi]
        push    r14
        push    r13
        push    r12
        push    rbp
        push    rbx
        sub     rsp, 40
        mov     r14, QWORD PTR 24[rdi]
        mov     QWORD PTR 16[rsp], rax
        cmp     r14, rax
        je      .L74
        mov     r13, rdi
        mov     r15, r14
.L2:
        mov     rdi, r15
        mov     r14, r15
        call    eastl::RBTreeIncrement(eastl::rbtree_node_base const*)@PLT
        mov     r12, rax
        cmp     rax, QWORD PTR 16[rsp]
        je      .L6
.L3:
        mov     rdi, QWORD PTR 40[r14]
        mov     rdx, QWORD PTR 40[r12]
        cmp     rdi, rdx
        je      .L22
        vmovss  xmm1, DWORD PTR 52[r12]
        vaddss  xmm2, xmm1, DWORD PTR 60[r12]
        xor     esi, esi
        vmovss  xmm0, DWORD PTR 52[r14]
        vcomiss xmm0, xmm2
        jnb     .L7
        vaddss  xmm0, xmm0, DWORD PTR 60[r14]
        vcomiss xmm1, xmm0
        jnb     .L7
        vmovss  xmm1, DWORD PTR 56[r12]
        vaddss  xmm2, xmm1, DWORD PTR 64[r12]
        vmovss  xmm0, DWORD PTR 56[r14]
        vcomiss xmm0, xmm2
        jnb     .L7
        vaddss  xmm0, xmm0, DWORD PTR 64[r14]
        vcomiss xmm1, xmm0
        jnb     .L7
        cmp     BYTE PTR 48[r14], 0
        mov     esi, 1
        je      .L7
        movzx   esi, BYTE PTR 48[r12]
        xor     esi, 1
.L7:
        mov     ebx, DWORD PTR 32[r12]
        mov     eax, DWORD PTR 32[r14]
        cmp     ebx, eax
        jnb     .L8
        mov     ecx, eax
        mov     eax, ebx
        mov     ebx, ecx
.L8:
        mov     rcx, rbx
        mov     ebx, DWORD PTR 8[rsp]
        mov     r15, QWORD PTR 96[r13]
        lea     rbp, 80[r13]
        sal     rcx, 32
        or      rbx, rcx
        movabs  rcx, -4294967296
        and     rbx, rcx
        or      rbx, rax
        mov     QWORD PTR 8[rsp], rbx
        test    r15, r15
        je      .L9
        mov     r9, rbp
        mov     rcx, r15
        jmp     .L13
.L10:
        test    rax, rax
        je      .L11
        mov     r9, rcx
.L12:
        mov     rcx, rax
.L13:
        mov     r8, QWORD PTR [rcx]
        mov     rax, QWORD PTR 8[rcx]
        cmp     QWORD PTR 32[rcx], rbx
        jnb     .L10
        test    r8, r8
        je      .L45
        mov     rax, r8
        jmp     .L12
.L19:
        cmp     rbp, r15
        je      .L22
        cmp     rbx, QWORD PTR 32[r15]
        jb      .L22
        dec     QWORD PTR 112[r13]
        mov     rdi, r15
        call    eastl::RBTreeIncrement(eastl::rbtree_node_base const*)@PLT
        mov     rsi, rbp
        mov     rdi, r15
        call    eastl::RBTreeErase(eastl::rbtree_node_base*, eastl::rbtree_node_base*)@PLT
        xor     esi, esi
        mov     rdi, r15
        call    Memory::free_static(void*, bool)@PLT
.L22:
        mov     rdi, r12
        call    eastl::RBTreeIncrement(eastl::rbtree_node_base const*)@PLT
        mov     r12, rax
        cmp     rax, QWORD PTR 16[rsp]
        jne     .L3
        mov     r15, r14
.L6:
        mov     rdi, r15
        call    eastl::RBTreeIncrement(eastl::rbtree_node_base const*)@PLT
        mov     r15, rax
        cmp     rax, QWORD PTR 16[rsp]
        jne     .L2
.L74:
        add     rsp, 40
        pop     rbx
        pop     rbp
        pop     r12
        pop     r13
        pop     r14
        pop     r15
        ret
.L45:
        mov     rcx, r9
.L11:
        cmp     rbp, rcx
        je      .L9
        cmp     rbx, QWORD PTR 32[rcx]
        jb      .L9
        test    sil, sil
        jne     .L22
        mov     rax, QWORD PTR 144[r13]
        test    rax, rax
        je      .L36
        mov     r15d, DWORD PTR 68[r12]
        mov     r8, QWORD PTR 40[rcx]
        mov     esi, DWORD PTR 68[r14]
        mov     r9, QWORD PTR 152[r13]
        mov     ecx, r15d
        call    rax
        mov     r15, QWORD PTR 96[r13]
        test    r15, r15
        je      .L22
.L36:
        mov     rdx, rbp
        jmp     .L21
.L18:
        test    rax, rax
        je      .L19
.L20:
        mov     rdx, r15
        mov     r15, rax
.L21:
        mov     rcx, QWORD PTR [r15]
        mov     rax, QWORD PTR 8[r15]
        cmp     QWORD PTR 32[r15], rbx
        jnb     .L18
        mov     r15, rdx
        test    rcx, rcx
        je      .L19
        mov     rax, rcx
        jmp     .L20
.L9:
        test    sil, sil
        je      .L22
        mov     rax, QWORD PTR 128[r13]
        xor     r15d, r15d
        test    rax, rax
        je      .L39
        mov     ecx, DWORD PTR 68[r12]
        mov     esi, DWORD PTR 68[r14]
        mov     r8, QWORD PTR 152[r13]
        call    rax
        mov     r15, rax
.L39:
        xor     esi, esi
        mov     edi, 48
        call    Memory::alloc_static(unsigned long, bool)@PLT
        mov     r8, rax
        test    al, 7
        je      .L23
        lea     rdi, .LC0[rip]
        mov     QWORD PTR 24[rsp], rax
        call    eastl::AssertionFailure(char const*)@PLT
        mov     r8, QWORD PTR 24[rsp]
.L23:
        test    r8, r8
        je      .L77
.L24:
        vpxor   xmm0, xmm0, xmm0
        mov     QWORD PTR 40[r8], r15
        mov     BYTE PTR 24[r8], 1
        vmovups XMMWORD PTR [r8], xmm0
        mov     r15, QWORD PTR 96[r13]
        mov     QWORD PTR 32[r8], rbx
        mov     QWORD PTR 16[r8], 0
        test    r15, r15
        je      .L25
        mov     rsi, rbx
.L30:
        mov     rax, QWORD PTR 32[r15]
        mov     rcx, QWORD PTR [r15]
        mov     rdx, QWORD PTR 8[r15]
        cmp     rbx, rax
        jb      .L26
.L78:
        test    rcx, rcx
        je      .L27
        mov     r15, rcx
        mov     rax, QWORD PTR 32[r15]
        mov     rcx, QWORD PTR [r15]
        mov     rdx, QWORD PTR 8[r15]
        cmp     rbx, rax
        jnb     .L78
.L26:
        test    rdx, rdx
        je      .L29
        mov     r15, rdx
        jmp     .L30
.L29:
        cmp     QWORD PTR 88[r13], r15
        je      .L32
.L37:
        mov     rdi, r15
        mov     QWORD PTR 24[rsp], r8
        call    eastl::RBTreeDecrement(eastl::rbtree_node_base const*)@PLT
        mov     r8, QWORD PTR 24[rsp]
        mov     rax, QWORD PTR 32[rax]
        mov     rsi, QWORD PTR 32[r8]
.L27:
        cmp     rsi, rax
        ja      .L32
        xor     esi, esi
        mov     rdi, r8
        call    Memory::free_static(void*, bool)@PLT
        jmp     .L22
.L32:
        xor     ecx, ecx
        cmp     rbp, r15
        je      .L31
        xor     ecx, ecx
        cmp     rsi, QWORD PTR 32[r15]
        setnb   cl
.L31:
        mov     rdx, rbp
        mov     rsi, r15
        mov     rdi, r8
        call    eastl::RBTreeInsert(eastl::rbtree_node_base*, eastl::rbtree_node_base*, eastl::rbtree_node_base*, eastl::RBTreeSide)@PLT
        inc     QWORD PTR 112[r13]
        jmp     .L22
.L77:
        lea     rdi, .LC1[rip]
        mov     QWORD PTR 24[rsp], r8
        call    eastl::AssertionFailure(char const*)@PLT
        mov     r8, QWORD PTR 24[rsp]
        jmp     .L24
.L25:
        mov     r15, QWORD PTR 88[r13]
        cmp     rbp, r15
        je      .L47
        mov     r15, rbp
        jmp     .L37
.L47:
        xor     ecx, ecx
        jmp     .L31
*/
BroadPhase2DBasic::ID BroadPhase2DBasic::create(CollisionObject2DSW *p_object_, int p_subindex) {

    current++;

    Element e;
    e.owner = p_object_;
    e._static = false;
    e.subindex = p_subindex;

    element_map[current] = e;
    return current;
}

void BroadPhase2DBasic::move(ID p_id, const Rect2 &p_aabb) {

    Map<ID, Element>::iterator E = element_map.find(p_id);
    ERR_FAIL_COND(E==element_map.end())
    E->second.aabb = p_aabb;
}
void BroadPhase2DBasic::set_static(ID p_id, bool p_static) {

    Map<ID, Element>::iterator E = element_map.find(p_id);
    ERR_FAIL_COND(E==element_map.end())
    E->second._static = p_static;
}
void BroadPhase2DBasic::remove(ID p_id) {

    Map<ID, Element>::iterator E = element_map.find(p_id);
    ERR_FAIL_COND(E==element_map.end())
    element_map.erase(E);
}

CollisionObject2DSW *BroadPhase2DBasic::get_object(ID p_id) const {

    Map<ID, Element>::const_iterator E = element_map.find(p_id);
    ERR_FAIL_COND_V(E==element_map.end(),nullptr)
    return E->second.owner;
}
bool BroadPhase2DBasic::is_static(ID p_id) const {

    Map<ID, Element>::const_iterator E = element_map.find(p_id);
    ERR_FAIL_COND_V(E==element_map.end(),false)
    return E->second._static;
}
int BroadPhase2DBasic::get_subindex(ID p_id) const {

    Map<ID, Element>::const_iterator E = element_map.find(p_id);
    ERR_FAIL_COND_V(E==element_map.end(),-1)
    return E->second.subindex;
}

int BroadPhase2DBasic::cull_segment(const Vector2 &p_from, const Vector2 &p_to, CollisionObject2DSW **p_results, int p_max_results, int *p_result_indices) {

    int rc = 0;

    for (eastl::pair<const ID,Element> &E : element_map) {

        const Rect2 aabb = E.second.aabb;
        if (aabb.intersects_segment(p_from, p_to)) {

            p_results[rc] = E.second.owner;
            p_result_indices[rc] = E.second.subindex;
            rc++;
            if (rc >= p_max_results)
                break;
        }
    }

    return rc;
}
int BroadPhase2DBasic::cull_aabb(const Rect2 &p_aabb, CollisionObject2DSW **p_results, int p_max_results, int *p_result_indices) {

    int rc = 0;

    for (eastl::pair<const ID,Element> &E : element_map) {

        const Rect2 aabb = E.second.aabb;
        if (aabb.intersects(p_aabb)) {

            p_results[rc] = E.second.owner;
            p_result_indices[rc] = E.second.subindex;
            rc++;
            if (rc >= p_max_results)
                break;
        }
    }

    return rc;
}

void BroadPhase2DBasic::set_pair_callback(PairCallback p_pair_callback, void *p_userdata) {

    pair_userdata = p_userdata;
    pair_callback = p_pair_callback;
}
void BroadPhase2DBasic::set_unpair_callback(UnpairCallback p_unpair_callback, void *p_userdata) {

    unpair_userdata = p_userdata;
    unpair_callback = p_unpair_callback;
}

void BroadPhase2DBasic::update() {

    // recompute pairs
    for (Map<ID,Element>::iterator I = element_map.begin(); I!=element_map.end(); ++I) {
        Map<ID, Element>::iterator J=I;
        // start from I+1
        for (++J; J!=element_map.end(); ++J) {

            Element *elem_A = &I->second;
            Element *elem_B = &J->second;

            if (elem_A->owner == elem_B->owner)
                continue;

            bool pair_ok = elem_A->aabb.intersects(elem_B->aabb) && (!elem_A->_static || !elem_B->_static);

            PairKey key(I->first, J->first);

            Map<PairKey, void *>::iterator E = pair_map.find(key);

            if (!pair_ok && E!=pair_map.end()) {
                if (unpair_callback)
                    unpair_callback(elem_A->owner, elem_A->subindex, elem_B->owner, elem_B->subindex, E->second, unpair_userdata);
                pair_map.erase(key);
            }

            if (pair_ok && E==pair_map.end()) {

                void *data = nullptr;
                if (pair_callback)
                    data = pair_callback(elem_A->owner, elem_A->subindex, elem_B->owner, elem_B->subindex, unpair_userdata);
                pair_map.emplace(key, data);
            }
        }
    }
}

BroadPhase2DSW *BroadPhase2DBasic::_create() {

    return memnew(BroadPhase2DBasic);
}

BroadPhase2DBasic::BroadPhase2DBasic() {

    current = 1;
    unpair_callback = nullptr;
    unpair_userdata = nullptr;
    pair_callback = nullptr;
    pair_userdata = nullptr;
}
