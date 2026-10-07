#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>

BlockBuffer::BlockBuffer(int blockNum) {
    this->blockNum = blockNum;
}

BlockBuffer::BlockBuffer(char blockType)
{
    if (blockType == 'R') {
        this->blockNum = getFreeBlock(REC);
    }
    else if (blockType == 'I') {
        this->blockNum = getFreeBlock(IND_INTERNAL);
    }
    else if (blockType == 'L') {
        this->blockNum = getFreeBlock(IND_LEAF);
    }
}

RecBuffer::RecBuffer()
    : BlockBuffer('R') {
}

RecBuffer::RecBuffer(int blockNum)
    : BlockBuffer(blockNum) {
}





int BlockBuffer::getHeader(struct HeadInfo *head) {
    unsigned char *bufferPtr;

    int status = loadBlockAndGetBufferPtr(&bufferPtr);

    if (status != SUCCESS) {
        return status;
    }

    // Extract the header fields from their fixed byte positions.
    memcpy(&head->blockType,  bufferPtr + 0,  4);
    memcpy(&head->pblock,     bufferPtr + 4,  4);
    memcpy(&head->lblock,     bufferPtr + 8,  4);
    memcpy(&head->rblock,     bufferPtr + 12, 4);
    memcpy(&head->numEntries, bufferPtr + 16, 4);
    memcpy(&head->numAttrs,   bufferPtr + 20, 4);
    memcpy(&head->numSlots,   bufferPtr + 24, 4);
    memcpy(head->reserved,    bufferPtr + 28, 4);

    return SUCCESS;
}

int RecBuffer::getSlotMap(unsigned char *slotMap)
{
    struct HeadInfo head;

    // First get the block header
    int status = this->getHeader(&head);

    if (status != SUCCESS)
    {
        return status;
    }

    // Get pointer to the block inside StaticBuffer
    unsigned char *bufferPtr;

    status = loadBlockAndGetBufferPtr(&bufferPtr);

    if (status != SUCCESS)
    {
        return status;
    }

    // Slot map starts immediately after the 32-byte block header
    unsigned char *slotMapInBuffer =
        bufferPtr + HEADER_SIZE;

    // Copy all slot-map entries
    memcpy(
        slotMap,
        slotMapInBuffer,
        head.numSlots
    );

    return SUCCESS;
}



int RecBuffer::getRecord(union Attribute *rec, int slotNum)
{
    struct HeadInfo head;

    // Get the number of attributes and slots from the block header.
    int status = this->getHeader(&head);

    if (status != SUCCESS) {
        return status;
    }

    // Get a pointer to this block from the StaticBuffer.
    unsigned char *bufferPtr;

    status = loadBlockAndGetBufferPtr(&bufferPtr);

    if (status != SUCCESS) {
        return status;
    }

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    // Every attribute occupies ATTR_SIZE bytes.
    int recordSize = attrCount * ATTR_SIZE;

    /*
     * Record area begins after:
     * 1. the block header
     * 2. the slot map
     *
     * Then move forward by slotNum records.
     */
    unsigned char *slotPointer =
        bufferPtr + HEADER_SIZE + slotCount
                  + (recordSize * slotNum);

    // Copy the complete record into the caller's array.
    memcpy(rec, slotPointer, recordSize);

    return SUCCESS;
}

int RecBuffer::setRecord(Attribute *rec, int slotNum)
{
    struct HeadInfo head;

    int status = this->getHeader(&head);

    if (status != SUCCESS)
    {
        return status;
    }

    if (slotNum < 0 || slotNum >= head.numSlots)
    {
        return E_OUTOFBOUND;
    }

    unsigned char *bufferPtr;

    status = loadBlockAndGetBufferPtr(&bufferPtr);

    if (status != SUCCESS)
    {
        return status;
    }

    int recordSize = head.numAttrs * ATTR_SIZE;

    unsigned char *slotPointer =
        bufferPtr +
        HEADER_SIZE +
        head.numSlots +
        (recordSize * slotNum);

    memcpy(slotPointer, rec, recordSize);

    status = StaticBuffer::setDirtyBit(this->blockNum);

    if (status != SUCCESS)
    {
        return status;
    }

    return SUCCESS;
}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr)
{
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

    if (bufferNum != E_BLOCKNOTINBUFFER)
    {
        // Block already in buffer:
        // make it most recently used
        for (int i = 0; i < BUFFER_CAPACITY; i++)
        {
            if (!StaticBuffer::metainfo[i].free)
            {
                StaticBuffer::metainfo[i].timeStamp++;
            }
        }

        StaticBuffer::metainfo[bufferNum].timeStamp = 0;
    }
    else
    {
        // Block not in buffer: allocate one
        bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

        if (bufferNum == E_OUTOFBOUND)
        {
            return E_OUTOFBOUND;
        }

        int status = Disk::readBlock(
            StaticBuffer::blocks[bufferNum],
            this->blockNum
        );

        if (status != SUCCESS)
        {
            return status;
        }
    }

    *buffPtr = StaticBuffer::blocks[bufferNum];

    return SUCCESS;
}



int compareAttrs(Attribute attr1, Attribute attr2, int attrType)
{
    double diff;

    if (attrType == STRING)
    {
        diff = strcmp(attr1.sVal, attr2.sVal);
    }
    else
    {
        diff = attr1.nVal - attr2.nVal;
    }

    if (diff > 0)
    {
        return 1;
    }
    else if (diff < 0)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

int BlockBuffer::setHeader(struct HeadInfo *head)
{
    unsigned char *bufferPtr;

    int status = loadBlockAndGetBufferPtr(&bufferPtr);

    if (status != SUCCESS) {
        return status;
    }

    struct HeadInfo *bufferHeader =
        (struct HeadInfo *)bufferPtr;

    bufferHeader->blockType  = head->blockType;
    bufferHeader->pblock     = head->pblock;
    bufferHeader->lblock     = head->lblock;
    bufferHeader->rblock     = head->rblock;
    bufferHeader->numEntries = head->numEntries;
    bufferHeader->numAttrs   = head->numAttrs;
    bufferHeader->numSlots   = head->numSlots;

    status = StaticBuffer::setDirtyBit(this->blockNum);

    if (status != SUCCESS) {
        return status;
    }

    return SUCCESS;
}

int BlockBuffer::setBlockType(int blockType)
{
    unsigned char *bufferPtr;

    // Make sure this block is present in the buffer
    int status = loadBlockAndGetBufferPtr(&bufferPtr);

    if (status != SUCCESS) {
        return status;
    }

    // First 4 bytes of every block contain blockType
    *((int32_t *)bufferPtr) = blockType;

    // Update the in-memory block allocation map also
    StaticBuffer::blockAllocMap[this->blockNum] = blockType;

    // We modified the buffer, so mark it dirty
    status = StaticBuffer::setDirtyBit(this->blockNum);

    if (status != SUCCESS) {
        return status;
    }

    return SUCCESS;
}

int BlockBuffer::getFreeBlock(int blockType)
{
    int freeBlock = -1;

    // Find an unused disk block.
    for (int i = 0; i < DISK_BLOCKS; i++) {
        if (StaticBuffer::blockAllocMap[i] == UNUSED_BLK) {
            freeBlock = i;
            break;
        }
    }

    // No free block exists.
    if (freeBlock == -1) {
        return E_DISKFULL;
    }

    // This BlockBuffer object now represents the new block.
    this->blockNum = freeBlock;

    // Allocate a RAM buffer for the new block.
    int bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

    if (bufferNum < 0) {
        return bufferNum;
    }

    // Initialise the header of the new block.
    struct HeadInfo head;

    head.blockType  = blockType;
    head.pblock     = -1;
    head.lblock     = -1;
    head.rblock     = -1;
    head.numEntries = 0;
    head.numAttrs   = 0;
    head.numSlots   = 0;

    int status = this->setHeader(&head);

    if (status != SUCCESS) {
        return status;
    }

    // Update both block header type and block allocation map.
    status = this->setBlockType(blockType);

    if (status != SUCCESS) {
        return status;
    }

    return this->blockNum;
}

int BlockBuffer::getBlockNum()
{
    return this->blockNum;
}

int RecBuffer::setSlotMap(unsigned char *slotMap)
{
    unsigned char *bufferPtr;

    // Make sure the block is loaded in memory
    int status = loadBlockAndGetBufferPtr(&bufferPtr);

    if (status != SUCCESS) {
        return status;
    }

    // Get block information
    struct HeadInfo head;

    status = this->getHeader(&head);

    if (status != SUCCESS) {
        return status;
    }

    int numSlots = head.numSlots;

    // Slot map starts immediately after the block header
    unsigned char *slotMapInBuffer =
        bufferPtr + HEADER_SIZE;

    // Replace existing slot map
    memcpy(
        slotMapInBuffer,
        slotMap,
        numSlots
    );

    // Buffer has been modified
    status = StaticBuffer::setDirtyBit(this->blockNum);

    if (status != SUCCESS) {
        return status;
    }

    return SUCCESS;
}