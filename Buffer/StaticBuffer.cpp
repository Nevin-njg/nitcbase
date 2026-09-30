#include "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];

struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

unsigned char StaticBuffer::blockAllocMap[DISK_BLOCKS];


StaticBuffer::StaticBuffer()
{
    // Load Block Allocation Map from disk into memory
    for (int blockNum = 0;
         blockNum < BLOCK_ALLOCATION_MAP_SIZE;
         blockNum++)
    {
        Disk::readBlock(
            blockAllocMap + blockNum * BLOCK_SIZE,
            blockNum
        );
    }

    // Initialize metadata of all buffer slots
    for (int bufferIndex = 0;
         bufferIndex < BUFFER_CAPACITY;
         bufferIndex++)
    {
        metainfo[bufferIndex].free = true;
        metainfo[bufferIndex].dirty = false;
        metainfo[bufferIndex].blockNum = -1;
        metainfo[bufferIndex].timeStamp = -1;
    }
}


StaticBuffer::~StaticBuffer()
{
    // Write Block Allocation Map back to disk
    for (int blockNum = 0;
         blockNum < BLOCK_ALLOCATION_MAP_SIZE;
         blockNum++)
    {
        Disk::writeBlock(
            blockAllocMap + blockNum * BLOCK_SIZE,
            blockNum
        );
    }

    // Write all dirty buffer blocks back to disk
    for (int bufferIndex = 0;
         bufferIndex < BUFFER_CAPACITY;
         bufferIndex++)
    {
        if (!metainfo[bufferIndex].free &&
            metainfo[bufferIndex].dirty)
        {
            Disk::writeBlock(
                blocks[bufferIndex],
                metainfo[bufferIndex].blockNum
            );
        }
    }
}


int StaticBuffer::getBufferNum(int blockNum)
{
    if (blockNum < 0 || blockNum > DISK_BLOCKS)
    {
        return E_OUTOFBOUND;
    }

    for (int bufferIndex = 0;
         bufferIndex < BUFFER_CAPACITY;
         bufferIndex++)
    {
        if (!metainfo[bufferIndex].free &&
            metainfo[bufferIndex].blockNum == blockNum)
        {
            return bufferIndex;
        }
    }

    return E_BLOCKNOTINBUFFER;
}

int StaticBuffer::getFreeBuffer(int blockNum)
{
    if (blockNum < 0 || blockNum >= DISK_BLOCKS)
    {
        return E_OUTOFBOUND;
    }

    // Increase timestamp of all occupied buffers
    for (int i = 0; i < BUFFER_CAPACITY; i++)
    {
        if (!metainfo[i].free)
        {
            metainfo[i].timeStamp++;
        }
    }

    int bufferNum = -1;

    // First try to find a free buffer
    for (int i = 0; i < BUFFER_CAPACITY; i++)
    {
        if (metainfo[i].free)
        {
            bufferNum = i;
            break;
        }
    }

    // If no free buffer, use LRU
    if (bufferNum == -1)
    {
        int maxTimeStamp = -1;

        for (int i = 0; i < BUFFER_CAPACITY; i++)
        {
            if (metainfo[i].timeStamp > maxTimeStamp)
            {
                maxTimeStamp = metainfo[i].timeStamp;
                bufferNum = i;
            }
        }

        // If LRU buffer is dirty, write it back to disk
        if (metainfo[bufferNum].dirty)
        {
            Disk::writeBlock(
                blocks[bufferNum],
                metainfo[bufferNum].blockNum
            );
        }
    }

    // Assign this buffer to the new block
    metainfo[bufferNum].free = false;
    metainfo[bufferNum].dirty = false;
    metainfo[bufferNum].blockNum = blockNum;
    metainfo[bufferNum].timeStamp = 0;

    return bufferNum;
}

int StaticBuffer::setDirtyBit(int blockNum)
{
    int bufferNum = getBufferNum(blockNum);

    if (bufferNum < 0)
    {
        return bufferNum;
    }

    metainfo[bufferNum].dirty = true;

    return SUCCESS;
}