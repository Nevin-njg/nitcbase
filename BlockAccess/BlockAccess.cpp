#include "BlockAccess.h"

#include <cstring>


RecId BlockAccess::linearSearch(
    int relId,
    char *attrName,
    Attribute attrVal,
    int op)
{
    // 1. Get where the previous search stopped
    RecId prevRecId;
    RelCacheTable::getSearchIndex(relId, &prevRecId);

    // 2. Get relation metadata
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    // 3. Get metadata of the attribute used in WHERE condition
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(
        relId,
        attrName,
        &attrCatEntry
    );

    int block;
    int slot;

    // 4. Decide where to start searching
    if (prevRecId.block == -1 &&
        prevRecId.slot == -1)
    {
        // New search -> start from first block, first slot
        block = relCatEntry.firstBlk;
        slot = 0;
    }
    else
    {
        // Continue after previous matching record
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }

    // 5. Search through record blocks
    while (block != -1)
    {
        RecBuffer blockBuffer(block);

        struct HeadInfo head;
        blockBuffer.getHeader(&head);

        // If all slots of this block are over,
        // move to the next record block
        if (slot >= head.numSlots)
        {
            block = head.rblock;
            slot = 0;
            continue;
        }

        // 6. Get slot map
        unsigned char slotMap[head.numSlots];
        blockBuffer.getSlotMap(slotMap);

        // Ignore empty slots
        if (slotMap[slot] == SLOT_UNOCCUPIED)
        {
            slot++;
            continue;
        }

        // 7. Read the actual record
        Attribute record[head.numAttrs];
        blockBuffer.getRecord(record, slot);

        // 8. Compare condition attribute with query value
        int cmpVal = compareAttrs(
            record[attrCatEntry.offset],
            attrVal,
            attrCatEntry.attrType
        );

        // 9. Check the required operator
        if (
            (op == NE && cmpVal != 0) ||
            (op == LT && cmpVal < 0)  ||
            (op == LE && cmpVal <= 0) ||
            (op == EQ && cmpVal == 0) ||
            (op == GT && cmpVal > 0)  ||
            (op == GE && cmpVal >= 0)
        )
        {
            // Matching record found
            RecId currentRecId = {block, slot};

            // Remember this location for the next call
            RelCacheTable::setSearchIndex(
                relId,
                &currentRecId
            );

            return currentRecId;
        }

        // Current record didn't match
        slot++;
    }

    // No more matching records
    return RecId{-1, -1};
}