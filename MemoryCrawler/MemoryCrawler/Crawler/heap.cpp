//
//  heap.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/6.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "heap.h"

int32_t HeapMemoryReader::seekOffset(address_t address)
{
    if (address == 0) {return -1;}
    if (address >= __startAddress && address < __stopAddress)
    {
        return (int32_t)(address - __startAddress);
    }
    
    auto heapIndex = findHeapOfAddress(address);
    if (heapIndex == -1) {return -1;}
    
    MemorySection &heap = __managedHeapSections->items[heapIndex];
    __memory = heap.bytes->items;
    __startAddress = heap.startAddress;
    __stopAddress = heap.startAddress + heap.bytes->size;
    __size = heap.bytes->size;
    
    return (int32_t)(address - __startAddress);
}

uint32_t HeapMemoryReader::readStringLength(address_t address)
{
    auto offset = seekOffset(address);
    if (offset == -1) {return 0;}
    
    auto length = readInt32(address);
    return length > 0 ? length : 0;
}

int32_t HeapMemoryReader::readString(address_t address, char16_t *buffer)
{
    auto offset = seekOffset(address);
    if (offset == -1) {return -1;}
    
    auto length = readInt32(address);
    if (length <= 0) {return -2;}
    
    offset += 4;
    memcpy(buffer, __memory + offset, length << 1);
    return length;
}

uint32_t HeapMemoryReader::readArrayLength(address_t address, TypeDescription &type)
{
    auto offset = seekOffset(address);
    if (offset == -1) {return 0;}
    
    auto bounds = readPointer(address + __vm->arrayBoundsOffsetInHeader);
    if (bounds == 0)
    {
        return (uint32_t)readPointer(address + __vm->arraySizeOffsetInHeader);
    }
    
    auto length = (uint64_t)1;
    auto cursor = bounds;
    for (auto i = 0; i < type.arrayRank; i++)
    {
        length *= readPointer(cursor);
        cursor += __vm->pointerSize;
    }
    return (uint32_t)length;
}

static const string sSystemString("System.String");
uint32_t HeapMemoryReader::readObjectSize(address_t address, TypeDescription &type)
{
    auto offset = seekOffset(address);
    if (offset == -1) {return 0;}
    if (type.isArray)
    {
        if (type.baseOrElementTypeIndex < 0 || type.baseOrElementTypeIndex >= __snapshot.typeDescriptions->size)
        {
            return 0;
        }
        auto elementCount = readArrayLength(address, type);
        auto elementType = __snapshot.typeDescriptions->items[type.baseOrElementTypeIndex];
        auto elementSize = elementType.isValueType ? elementType.size : __vm->pointerSize;
        return __vm->arrayHeaderSize + elementSize * elementCount;
    }
    
    if ((type.typeIndex >= 0 && type.typeIndex == __snapshot.managedTypeIndex.system_String)
        || sSystemString == *type.name)
    {
        auto size = __vm->objectHeaderSize;
        size += 4; // string length
        size += readStringLength(address + __vm->objectHeaderSize) * 2; // char16_t
        size += 2; // \x00\x00
        return size;
    }
    
    return type.size;
}

HeapSegment HeapMemoryReader::readObjectMemory(address_t address, TypeDescription &type)
{
    HeapSegment segment;
    auto size = readObjectSize(address, type);
    if (size <= 0) { return segment; }
    
    auto offset = seekOffset(address);
    if (offset < 0) { return segment; }
    
    segment.begin = (char *)__memory + offset;
    segment.end = segment.begin + size;
    return segment;
}

int32_t HeapMemoryReader::findHeapOfAddress(address_t address)
{
    int32_t min = 0, max = __managedHeapSections->size - 1;
    Array<MemorySection> &heapSections = *__managedHeapSections;
    
    while (min <= max)
    {
        auto mid = (min + max) >> 1;
        MemorySection &heap = heapSections[mid];
        if (heap.startAddress > address)
        {
            max = mid - 1;
        }
        else if (heap.startAddress + heap.bytes->size < address)
        {
            min = mid + 1;
        }
        else
        {
            return mid;
        }
    }
    
    return -1;
}

void StaticMemoryReader::load(const byte_t *bytes, int32_t size)
{
    __memory = bytes;
    __startAddress = 0;
    __stopAddress = size;
    this->__size = size;
}

int32_t StaticMemoryReader::seekOffset(address_t address)
{
    if (address < 0 || __size == 0 || address >= __size) {return -1;}
    return (int32_t)address;
}
