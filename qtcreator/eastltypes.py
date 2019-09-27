############################################################################
#
# Copyright (C) 2016 The Qt Company Ltd.
# Contact: https://www.qt.io/licensing/
#
# This file is part of Qt Creator.
#
# Commercial License Usage
# Licensees holding valid commercial Qt licenses may use this file in
# accordance with the commercial license agreement provided with the
# Software or, alternatively, in accordance with the terms contained in
# a written agreement between you and The Qt Company. For licensing terms
# and conditions see https://www.qt.io/terms-conditions. For further
# information use the contact form at https://www.qt.io/contact-us.
#
# GNU General Public License Usage
# Alternatively, this file may be used under the terms of the GNU
# General Public License version 3 as published by the Free Software
# Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
# included in the packaging of this file. Please review the following
# information to ensure the GNU General Public License requirements will
# be met: https://www.gnu.org/licenses/gpl-3.0.html.
#
############################################################################

from dumper import *

def qform__eastl__array():
    return arrayForms()

def qdump__eastl__array(d, value):
    size = value.type[1]
    d.putItemCount(size)
    if d.isExpanded():
        d.putPlotData(value.address(), size, value.type[0])


def qdump__eastl__function(d, value):
    (ptr, dummy1, manager, invoker) = value.split('pppp')
    if manager:
        if ptr > 2:
            d.putSymbolValue(ptr)
        else:
            d.putEmptyValue()
        d.putBetterType(value.type)
    else:
        d.putValue('(null)')
    d.putPlainChildren(value)


def qdump__eastl__complex(d, value):
    innerType = value.type[0]
    (real, imag) = value.split('{%s}{%s}' % (innerType.name, innerType.name))
    d.putValue("(%s, %s)" % (real.display(), imag.display()))
    d.putNumChild(2)
    if d.isExpanded():
        with Children(d, 2, childType=innerType):
            d.putSubItem("real", real)
            d.putSubItem("imag", imag)

def qdump__eastl____1__complex(d, value):
    qdump__eastl__complex(d, value)

def qdump__eastl__DequeBase(d, value):
    #if d.isMsvcTarget():
        #qdump__eastl__deque__MSVC(d, value)
        #return

    bufsize = 1
    innerType = d.templateArgument(value.type, 0)
    innerSize = innerType.size()
    blockSize = d.templateArgument(value.type, 2)
    if innerSize < 512:
        bufsize = 512 // innerSize

    startCur = value["mItBegin"]
    startArrayPtr = startCur["mpCurrentArrayPtr"]

    size = (value["mItEnd"]["mpCurrentArrayPtr"] - startCur["mpCurrentArrayPtr"]) * blockSize + (value["mItEnd"]["mpCurrent"]-value["mItEnd"]["mpBegin"]) - (startCur["mpCurrent"]-startCur["mpBegin"])

    d.check(0 <= size and size <= 1000 * 1000 * 1000)
    d.putItemCount(size)
    if d.isExpanded():
        with Children(d, size, maxNumChild=2000, childType=innerType):
            for i in d.childRange():
                pcur = startArrayPtr[int((startCur["mpCurrent"]-startCur["mpBegin"]+i)/blockSize)]
                sub = (startCur["mpCurrent"]-startCur["mpBegin"] + i) % blockSize
                d.putSubItem(i,pcur[sub])

def qdump__eastl____1__deque(d, value):
    mptr, mfirst, mbegin, mend, start, size = value.split("pppptt")
    d.check(0 <= size and size <= 1000 * 1000 * 1000)
    d.putItemCount(size)
    if d.isExpanded():
        innerType = value.type[0]
        innerSize = innerType.size()
        ptrSize = d.ptrSize()
        bufsize = (4096 // innerSize) if innerSize < 256 else 16
        with Children(d, size, maxNumChild=2000, childType=innerType):
            for i in d.childRange():
                k, j = divmod(start + i, bufsize)
                base = d.extractPointer(mfirst + k * ptrSize)
                d.putSubItem(i, d.createValue(base + j * innerSize, innerType))

def qdump__eastl__list(d, value):
    if d.isQnxTarget() or d.isMsvcTarget():
        qdump__eastl__list__QNX(d, value)
        return

    if value.type.size() == 3 * d.ptrSize():
        # C++11 only.
        (dummy1, dummy2, size) = value.split("ppp")
        d.putItemCount(size)
    else:
        # Need to count manually.
        p = d.extractPointer(value)
        head = value.address()
        size = 0
        while head != p and size < 1001:
            size += 1
            p = d.extractPointer(p)
        d.putItemCount(size, 1000)

    if d.isExpanded():
        p = d.extractPointer(value)
        innerType = value.type[0]
        with Children(d, size, maxNumChild=1000, childType=innerType):
            for i in d.childRange():
                d.putSubItem(i, d.createValue(p + 2 * d.ptrSize(), innerType))
                p = d.extractPointer(p)

def qdump__eastl__list__QNX(d, value):
    (proxy, head, size) = value.split("ppp")
    d.putItemCount(size, 1000)

    if d.isExpanded():
        p = d.extractPointer(head)
        innerType = value.type[0]
        with Children(d, size, maxNumChild=1000, childType=innerType):
            for i in d.childRange():
                d.putSubItem(i, d.createValue(p + 2 * d.ptrSize(), innerType))
                p = d.extractPointer(p)

def qform__eastl__map():
    return mapForms()

def qdump__eastl__map(d, value):
    if d.isQnxTarget() or d.isMsvcTarget():
        qdump_eastl__map__helper(d, value)
        return

    # stuff is actually (color, pad) with 'I@', but we can save cycles/
    (compare, stuff, parent, left, right, size) = value.split('pppppp')
    d.check(0 <= size and size <= 100*1000*1000)
    d.putItemCount(size)

    if d.isExpanded():
        keyType = value.type[0]
        valueType = value.type[1]
        with Children(d, size, maxNumChild=1000):
            node = value["_M_t"]["_M_impl"]["_M_header"]["_M_left"]
            nodeSize = node.dereference().type.size()
            typeCode = "@{%s}@{%s}" % (keyType.name, valueType.name)
            for i in d.childRange():
                (pad1, key, pad2, value) = d.split(typeCode, node.pointer() + nodeSize)
                d.putPairItem(i, (key, value))
                if node["_M_right"].pointer() == 0:
                    parent = node["_M_parent"]
                    while True:
                        if node.pointer() != parent["_M_right"].pointer():
                            break
                        node = parent
                        parent = parent["_M_parent"]
                    if node["_M_right"] != parent:
                        node = parent
                else:
                    node = node["_M_right"]
                    while True:
                        if node["_M_left"].pointer() == 0:
                            break
                        node = node["_M_left"]

def qdump_eastl__map__helper(d, value):
    (proxy, head, size) = value.split("ppp")
    d.check(0 <= size and size <= 100*1000*1000)
    d.putItemCount(size)
    if d.isExpanded():
        keyType = value.type[0]
        valueType = value.type[1]
        pairType = value.type[3][0]
        def helper(node):
            (left, parent, right, color, isnil, pad, pair) = d.split("pppcc@{%s}" % (pairType.name), node)
            if left != head:
                for res in helper(left):
                    yield res
            yield pair.split("{%s}@{%s}" % (keyType.name, valueType.name))[::2]
            if right != head:
                for res in helper(right):
                    yield res

        (smallest, root) = d.split("pp", head)
        with Children(d, size, maxNumChild=1000):
            for (pair, i) in zip(helper(root), d.childRange()):
                d.putPairItem(i, pair)

def qdump__eastl____debug__map(d, value):
    qdump__eastl__map(d, value)

def qdump__eastl____debug__set(d, value):
    qdump__eastl__set(d, value)

def qdump__eastl__multiset(d, value):
    qdump__eastl__set(d, value)

def qdump__eastl____cxx1998__map(d, value):
    qdump__eastl__map(d, value)

def qform__eastl__multimap():
    return mapForms()

def qdump__eastl__multimap(d, value):
    return qdump__eastl__map(d, value)

def qdumpHelper__eastl__tree__iterator(d, value, isSet=False):
    if value.type.name.endswith("::iterator"):
        treeTypeName = value.type.name[:-len("::iterator")]
    elif value.type.name.endswith("::const_iterator"):
        treeTypeName = value.type.name[:-len("::const_iterator")]
    treeType = d.lookupType(treeTypeName)
    keyType = treeType[0]
    valueType = treeType[1]
    node = value["_M_node"].dereference()   # std::_Rb_tree_node_base
    d.putNumChild(1)
    d.putEmptyValue()
    if d.isExpanded():
        with Children(d):
            if isSet:
                typecode = 'pppp@{%s}' % keyType.name
                (color, parent, left, right, pad1, key) = d.split(typecode, node)
                d.putSubItem("value", key)
            else:
                typecode = 'pppp@{%s}@{%s}' % (keyType.name, valueType.name)
                (color, parent, left, right, pad1, key, pad2, value) = d.split(typecode, node)
                d.putSubItem("first", key)
                d.putSubItem("second", value)
            with SubItem(d, "[node]"):
                d.putNumChild(1)
                d.putEmptyValue()
                d.putType(" ")
                if d.isExpanded():
                    with Children(d):
                        #d.putSubItem("color", color)
                        nodeType = node.type.pointer()
                        d.putSubItem("left", d.createValue(left, nodeType))
                        d.putSubItem("right", d.createValue(right, nodeType))
                        d.putSubItem("parent", d.createValue(parent, nodeType))

def qdump__eastl___Rb_tree_iterator(d, value):
    qdumpHelper__eastl__tree__iterator(d, value)

def qdump__eastl___Rb_tree_const_iterator(d, value):
    qdumpHelper__eastl__tree__iterator(d, value)

def qdump__eastl__map__iterator(d, value):
    qdumpHelper__eastl__tree__iterator(d, value)

def qdump____gnu_debug___Safe_iterator(d, value):
    d.putItem(value["_M_current"])

def qdump__eastl__map__const_iterator(d, value):
    qdumpHelper__eastl__tree__iterator(d, value)

def qdump__eastl__set__iterator(d, value):
    qdumpHelper__eastl__tree__iterator(d, value, True)

def qdump__eastl__set__const_iterator(d, value):
    qdumpHelper__eastl__tree__iterator(d, value, True)

def qdump__eastl____cxx1998__set(d, value):
    qdump__eastl__set(d, value)

def qdumpHelper__eastl__tree__iterator_MSVC(d, value):
    d.putNumChild(1)
    d.putEmptyValue()
    if d.isExpanded():
        with Children(d):
            childType = value.type[0][0][0]
            (proxy, nextIter, node) = value.split("ppp")
            (left, parent, right, color, isnil, pad, child) = \
                d.split("pppcc@{%s}" % (childType.name), node)
            if (childType.name.startswith("eastl::pair")):
                # workaround that values created via split have no members
                keyType = childType[0].name
                valueType = childType[1].name
                d.putPairItem(None, child.split("{%s}@{%s}" % (keyType, valueType))[::2])
            else:
                d.putSubItem("value", child)

def qdump__eastl___Tree_const_iterator(d, value):
    qdumpHelper__eastl__tree__iterator_MSVC(d, value)

def qdump__eastl___Tree_iterator(d, value):
    qdumpHelper__eastl__tree__iterator_MSVC(d, value)


def qdump__eastl__set(d, value):
    if d.isQnxTarget() or d.isMsvcTarget():
        qdump__eastl__set__QNX(d, value)
        return

    impl = value
    size = value["mnSize"].integer()
    d.check(0 <= size and size <= 100*1000*1000)
    d.putItemCount(size)
    if d.isExpanded():
        valueType = value.type[0]
        node = impl["mAnchor"]["mpNodeLeft"]
        nodeSize = node.dereference().type.size()
        typeCode = "@{%s}" % valueType.name
        with Children(d, size, maxNumChild=1000, childType=valueType):
            for i in d.childRange():
                (pad, val) = d.split(typeCode, node.pointer() + nodeSize)
                d.putSubItem(i, val)
                if node["mpNodeRight"].pointer() == 0:
                    parent = node["mpNodeParent"]
                    while node == parent["mpNodeRight"]:
                        node = parent
                        parent = parent["mpNodeParent"]
                    if node["mpNodeRight"] != parent:
                        node = parent
                else:
                    node = node["mpNodeRight"]
                    while node["mpNodeLeft"].pointer() != 0:
                        node = node["mpNodeLeft"]

def eastl1TreeMin(d, node):
    #_NodePtr __tree_min(_NodePtr __x):
    #    while (__x->__left_ != nullptr)
    #        __x = __x->__left_;
    #    return __x;
    #
    left = node['__left_']
    if left.pointer():
        node = left
    return node

def eastl1TreeIsLeftChild(d, node):
    # bool __tree_is_left_child(_NodePtr __x):
    #     return __x == __x->__parent_->__left_;
    #
    other = node['__parent_']['__left_']
    return node.pointer() == other.pointer()


def eastl1TreeNext(d, node):
    #_NodePtr __tree_next(_NodePtr __x):
    #    if (__x->__right_ != nullptr)
    #        return __tree_min(__x->__right_);
    #    while (!__tree_is_left_child(__x))
    #        __x = __x->__parent_;
    #    return __x->__parent_;
    #
    right = node['__right_']
    if right.pointer():
        return eastl1TreeMin(d, right)
    while not eastl1TreeIsLeftChild(d, node):
        node = node['__parent_']
    return node['__parent_']

def qdump__eastl__stack(d, value):
    d.putItem(value["c"])
    d.putBetterType(value.type)

def qdump__eastl____debug__stack(d, value):
    qdump__eastl__stack(d, value)

def qdump__eastl____1__stack(d, value):
    d.putItem(value["c"])
    d.putBetterType(value.type)

def qform__eastl__string():
    return [Latin1StringFormat, SeparateLatin1StringFormat,
            Utf8StringFormat, SeparateUtf8StringFormat ]

def qdump__eastl__string(d, value):
    qdumpHelper_eastl__string(d, value, d.createType("char"), d.currentItemFormat())

def qdumpHelper_eastl__string(d, value, charType, format):
    if d.isQnxTarget():
        qdumpHelper__eastl__string__QNX(d, value, charType, format)
        return
    if d.isMsvcTarget():
        qdumpHelper__eastl__string__MSVC(d, value, charType, format)
        return

    data = value.extractPointer()
    # We can't lookup the eastl::string::_Rep type without crashing LLDB,
    # so hard-code assumption on member position
    # struct { size_type _M_length, size_type _M_capacity, int _M_refcount; }
    (size, alloc, refcount) = d.split("ppp", data - 3 * d.ptrSize())
    refcount = refcount & 0xffffffff
    d.check(refcount >= -1) # Can be -1 according to docs.
    d.check(0 <= size and size <= alloc and alloc <= 100*1000*1000)
    d.putCharArrayHelper(data, size, charType, format)

def qdumpHelper__eastl__string__QNX(d, value, charType, format):
    size = value['_Mysize']
    alloc = value['_Myres']
    _BUF_SIZE = int(16 / charType.size())
    if _BUF_SIZE <= alloc: #(_BUF_SIZE <= _Myres ? _Bx._Ptr : _Bx._Buf);
        data = value['_Bx']['_Ptr']
    else:
        data = value['_Bx']['_Buf']
    sizePtr = data.cast(d.charType().pointer())
    refcount = int(sizePtr[-1])
    d.check(refcount >= -1) # Can be -1 accoring to docs.
    d.check(0 <= size and size <= alloc and alloc <= 100*1000*1000)
    d.putCharArrayHelper(sizePtr, size, charType, format)

def qdumpHelper__eastl__string__MSVC(d, value, charType, format):
    (proxy, buffer, size, alloc) = value.split("p16spp");
    _BUF_SIZE = int(16 / charType.size());
    d.check(0 <= size and size <= alloc and alloc <= 100*1000*1000)
    if _BUF_SIZE <= alloc:
        (proxy, data) = value.split("pp");
    else:
        data = value.address() + d.ptrSize()
    d.putCharArrayHelper(data, size, charType, format)

def qdump__eastl____1__string(d, value):
    firstByte = value.split('b')[0]
    if int(firstByte & 1) == 0:
        # Short/internal.
        size = int(firstByte / 2)
        data = value.address() + 1
    else:
        # Long/external.
        (dummy, size, data) = value.split('ppp')
    d.putCharArrayHelper(data, size, d.charType(), d.currentItemFormat())
    d.putType("eastl::string")


def qdump__eastl____1__wstring(d, value):
    firstByte = value.split('b')[0]
    if int(firstByte & 1) == 0:
        # Short/internal.
        size = int(firstByte / 2)
        data = value.address() + 4
    else:
        # Long/external.
        (dummy, size, data) = value.split('ppp')
    d.putCharArrayHelper(data, size, d.createType('wchar_t'))
    d.putType("eastl::wstring")


def qdump__eastl____weak_ptr(d, value):
    return qdump__eastl__shared_ptr(d, value)

def qdump__eastl__weak_ptr(d, value):
    return qdump__eastl__shared_ptr(d, value)

def qdump__eastl____1__weak_ptr(d, value):
    return qdump__eastl____1__shared_ptr(d, value)


def qdump__eastl__shared_ptr(d, value):
    if d.isMsvcTarget():
        i = value["_Ptr"]
    else:
        i = value["_M_ptr"]

    if i.pointer() == 0:
        d.putValue("(null)")
        d.putNumChild(0)
    else:
        d.putItem(i.dereference())
        d.putBetterType(value.type)

def qdump__eastl____1__shared_ptr(d, value):
    i = value["__ptr_"]
    if i.pointer() == 0:
        d.putValue("(null)")
        d.putNumChild(0)
    else:
        d.putItem(i.dereference())
        d.putBetterType(value.type)

def qdump__eastl__unique_ptr(d, value):
    p = d.extractPointer(value)
    if p == 0:
        d.putValue("(null)")
        d.putNumChild(0)
    else:
        d.putItem(d.createValue(p, value.type[0]))
        d.putBetterType(value.type)

def qdump__eastl____1__unique_ptr(d, value):
    qdump__eastl__unique_ptr(d, value)


def qdump__eastl__pair(d, value):
    typeCode = '{%s}@{%s}' % (value.type[0].name, value.type[1].name)
    first = value["first"]
    second = value["second"]
    with Children(d):
        key = d.putSubItem('first', first)
        value = d.putSubItem('second', second)
    d.putField('key', key.value)
    if key.encoding is not None:
        d.putField('keyencoded', key.encoding)
    d.putValue(value.value, value.encoding)

def qform__eastl__hashtable():
    return mapForms()

def qform__eastl__unordered_map():
    return mapForms()

def qform__eastl____debug__hashtable():
    return mapForms()

def qdump__eastl__hashtable(d, value):
    size = value["mnElementCount"].integer()
    start = value["mpBucketArray"]
    keyType = value.type.stripTypedefs()[0]
    valueType = value.type.stripTypedefs()[1]

    d.putItemCount(size)
    if d.isExpanded():
        i = 0
        bucket = start;
        node = bucket.dereference()
        while node.pointer() == 0:
            bucket=bucket+1
            node = bucket.dereference()

        with Children(d, size, maxNumChild=10000):
            for i in d.childRange():
                #d.putPairItem(i, node["mValue"].split("{%s}{%s}" % (keyType.name, valueType.name))[::2], 'key', 'value')
                d.putSubItem(i,node["mValue"])
                node = node["mpNext"]
                while node.pointer() == 0:
                    bucket=bucket+1
                    node = bucket.dereference()

def qdump__eastl__hash_map(d, value):
    qdump__eastl__hashtable(d, value)

def qdump__eastl__unordered_map(d, value):
    qdump__eastl__hash_map(d,v)



def qdump__eastl____debug__unordered_map(d, value):
    qdump__eastl__unordered_map(d, value)


def qform__eastl__unordered_multimap():
    return qform__eastl__unordered_map()

def qform__eastl____debug__unordered_multimap():
    return qform__eastl____debug__unordered_map()

def qdump__eastl__unordered_multimap(d, value):
    qdump__eastl__unordered_map(d, value)

def qdump__eastl____debug__unordered_multimap(d, value):
    qdump__eastl__unordered_multimap(d, value)


def qdump__eastl__unordered_set(d, value):
    if d.isQnxTarget() or d.isMsvcTarget():
        qdump__eastl__list__QNX(d, value["_List"])
        return

    try:
        # gcc ~= 4.7
        size = value["_M_element_count"].integer()
        start = value["_M_before_begin"]["_M_nxt"]
        offset = 0
    except:
        try:
            # libc++ (Mac)
            size = value["_M_h"]["_M_element_count"].integer()
            start = value["_M_h"]["_M_bbegin"]["_M_node"]["_M_nxt"]
            offset = 0
        except:
            try:
                # gcc 4.6.2
                size = value["_M_element_count"].integer()
                start = value["_M_buckets"].dereference()
                offset = d.ptrSize()
            except:
                # gcc 4.9.1
                size = value["_M_h"]["_M_element_count"].integer()
                start = value["_M_h"]["_M_before_begin"]["_M_nxt"]
                offset = 0

    d.putItemCount(size)
    if d.isExpanded():
        p = start.pointer()
        valueType = value.type[0]
        with Children(d, size, childType=valueType):
            ptrSize = d.ptrSize()
            for i in d.childRange():
                d.putSubItem(i, d.createValue(p + ptrSize - offset, valueType))
                p = d.extractPointer(p + offset)

def qform__eastl____1__unordered_map():
    return mapForms()

def qdump__eastl____1__unordered_map(d, value):
    (size, _) = value["__table_"]["__p2_"].split("pp")
    d.putItemCount(size)

    keyType = value.type[0]
    valueType = value.type[1]
    pairType = value.type[4][0]

    if d.isExpanded():
        curr = value["__table_"]["__p1_"].split("pp")[0]

        def traverse_list(node):
            while node:
                (next_, _, pad, pair) = d.split("pp@{%s}" % (pairType.name), node)
                yield pair.split("{%s}@{%s}" % (keyType.name, valueType.name))[::2]
                node = next_

        with Children(d, size, childType=value.type[0], maxNumChild=1000):
            for (i, value) in zip(d.childRange(), traverse_list(curr)):
                d.putPairItem(i, value, 'key', 'value')


def qdump__eastl____1__unordered_set(d, value):
    (size, _) = value["__table_"]["__p2_"].split("pp")
    d.putItemCount(size)

    valueType = value.type[0]

    if d.isExpanded():
        curr = value["__table_"]["__p1_"].split("pp")[0]

        def traverse_list(node):
            while node:
                (next_, _, pad, val) = d.split("pp@{%s}" % (valueType.name), node)
                yield val
                node = next_

        with Children(d, size, childType=value.type[0], maxNumChild=1000):
            for (i, value) in zip(d.childRange(), traverse_list(curr)):
                d.putSubItem(i, value)


def qdump__eastl____debug__unordered_set(d, value):
    qdump__eastl__unordered_set(d, value)

def qdump__eastl__unordered_multiset(d, value):
    qdump__eastl__unordered_set(d, value)

def qdump__eastl____debug__unordered_multiset(d, value):
    qdump__eastl__unordered_multiset(d, value)


def qform__eastl__valarray():
    return arrayForms()

def qdump__eastl__valarray(d, value):
    if d.isMsvcTarget():
        (data, size) = value.split('pp')
    else:
        (size, data) = value.split('pp')
    d.putItemCount(size)
    d.putPlotData(data, size, value.type[0])


def qform__eastl____1__valarray():
    return arrayForms()

def qdump__eastl____1__valarray(d, value):
    innerType = value.type[0]
    (begin, end) = value.split('pp')
    size = int((end - begin) / innerType.size())
    d.putItemCount(size)
    d.putPlotData(begin, size, innerType)


def qform__eastl__VectorBase():
    return arrayForms()

def qdump__eastl__VectorBase(d, value):
    innerType = value.type[0]
    start = value["mpBegin"]
    finish = value["mpEnd"]
    allocator = value["mCapacityAllocator"]
    size = int((finish - start))
    if size > 0:
        d.checkPointer(start)
        d.checkPointer(finish)
        d.checkPointer(allocator)

    d.check(0 <= size and size <= 1000 * 1000 * 1000)
    d.putItemCount(size)
    if d.isExpanded():
        with Children(d, size, maxNumChild=10000, childType=innerType):
            for i in d.childRange():
                d.putSubItem(i,start[i])

def qform__eastl____1__vector():
    return arrayForms()

def qdump__eastl____1__vector(d, value):
    qdumpHelper__eastl__vector(d, value, True)

def qform__eastl____debug__vector():
    return arrayForms()

def qdump__eastl____debug__vector(d, value):
    qdump__eastl__vector(d, value)


def qedit__eastl__string(d, value, data):
    d.call('void', value, 'assign', '"%s"' % data.replace('"', '\\"'))

def qedit__string(d, expr, value):
    qedit__eastl__string(d, expr, value)

def qedit__eastl____cxx11__string(d, expr, value):
    qedit__eastl__string(d, expr, value)

def qedit__eastl__wstring(d, value, data):
    d.call('void', value, 'assign', 'L"%s"' % data.replace('"', '\\"'))

def qedit__wstring(d, expr, value):
    qedit__eastl__wstring(d, expr, value)

def qedit__eastl____cxx11__wstring(d, expr, value):
    qedit__eastl__wstring(d, expr, value)

def qdump__string(d, value):
    qdump__eastl__string(d, value)

def qform__eastl__wstring():
    return [SimpleFormat, SeparateFormat]

def qdump__eastl__wstring(d, value):
    qdumpHelper_eastl__string(d, value, d.createType('wchar_t'), d.currentItemFormat())

def qdump__eastl__basic_string(d, value):
    innerType = value.type[0]
    qdumpHelper_eastl__string(d, value, innerType, d.currentItemFormat())

def qdump__wstring(d, value):
    qdump__eastl__wstring(d, value)

def qdump__eastl__once_flag(d, value):
    d.putValue(value.split("i")[0])
    d.putBetterType(value.type)
    d.putPlainChildren(value)


def qdump__eastl__optional(d, value):
    innerType = value.type[0]
    (initialized, pad, payload) = d.split('b@{%s}' % innerType.name, value)
    if initialized:
        d.putItem(payload)
        d.putBetterType(value.type)
    else:
        d.putSpecialValue("uninitialized")
        d.putNumChild(0)

def qdump__eastl__experimental__optional(d, value):
    qdump__eastl__optional(d, value)
