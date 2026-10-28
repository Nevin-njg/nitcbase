#include "BlockAccess.h"
#include <vector>
#include <cstring>

long long stage10LinearComparisons = 0;

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

        stage10LinearComparisons++;

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


int BlockAccess::renameRelation(char *oldName, char *newName)
{
    //  Check whether newName already exists
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute newRelationName;
    strcpy(newRelationName.sVal, newName);

    RecId newNameRecId = linearSearch(
        RELCAT_RELID,
        (char *)RELCAT_ATTR_RELNAME,
        newRelationName,
        EQ
    );

    if (newNameRecId.block != -1 &&
        newNameRecId.slot != -1)
    {
        return E_RELEXIST;
    }


    //  Find oldName
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute oldRelationName;
    strcpy(oldRelationName.sVal, oldName);

    RecId oldNameRecId = linearSearch(
        RELCAT_RELID,
        (char *)RELCAT_ATTR_RELNAME,
        oldRelationName,
        EQ
    );

    if (oldNameRecId.block == -1 &&
        oldNameRecId.slot == -1)
    {
        return E_RELNOTEXIST;
    }


    //  Read relation catalog record
    RecBuffer relCatBuffer(oldNameRecId.block);

    Attribute relCatRecord[RELCAT_NO_ATTRS];

    int ret = relCatBuffer.getRecord(
        relCatRecord,
        oldNameRecId.slot
    );

    if (ret != SUCCESS)
    {
        return ret;
    }


    //  Save number of attributes
    int numAttrs =
        (int)relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;


    //  Rename relation in RELATIONCAT
    strcpy(
        relCatRecord[RELCAT_REL_NAME_INDEX].sVal,
        newName
    );

    ret = relCatBuffer.setRecord(
        relCatRecord,
        oldNameRecId.slot
    );

    if (ret != SUCCESS)
    {
        return ret;
    }


    //  Rename relation name in all ATTRIBUTECAT entries
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    for (int i = 0; i < numAttrs; i++)
    {
        RecId attrRecId = linearSearch(
            ATTRCAT_RELID,
            (char *)ATTRCAT_ATTR_RELNAME,
            oldRelationName,
            EQ
        );

        RecBuffer attrCatBuffer(attrRecId.block);

        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

        ret = attrCatBuffer.getRecord(
            attrCatRecord,
            attrRecId.slot
        );

        if (ret != SUCCESS)
        {
            return ret;
        }

        strcpy(
            attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,
            newName
        );

        ret = attrCatBuffer.setRecord(
            attrCatRecord,
            attrRecId.slot
        );

        if (ret != SUCCESS)
        {
            return ret;
        }
    }

    return SUCCESS;
}

int BlockAccess::renameAttribute(
    char *relName,
    char *oldName,
    char *newName
)
{
    //  Check whether relation exists
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr;
    strcpy(relNameAttr.sVal, relName);

    RecId relCatRecId = linearSearch(
        RELCAT_RELID,
        (char *)RELCAT_ATTR_RELNAME,
        relNameAttr,
        EQ
    );

    if (relCatRecId.block == -1 &&
        relCatRecId.slot == -1)
    {
        return E_RELNOTEXIST;
    }


    //  Search attributes belonging to this relation
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    RecId attrToRenameRecId{-1, -1};

    Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];

    while (true)
    {
        RecId attrCatRecId = linearSearch(
            ATTRCAT_RELID,
            (char *)ATTRCAT_ATTR_RELNAME,
            relNameAttr,
            EQ
        );

        // No more attributes
        if (attrCatRecId.block == -1 &&
            attrCatRecId.slot == -1)
        {
            break;
        }

        RecBuffer attrCatBuffer(attrCatRecId.block);

        int ret = attrCatBuffer.getRecord(
            attrCatEntryRecord,
            attrCatRecId.slot
        );

        if (ret != SUCCESS)
        {
            return ret;
        }


        //  newName already exists
        if (
            strcmp(
                attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,
                newName
            ) == 0
        )
        {
            return E_ATTREXIST;
        }


        //  Remember where oldName is stored
        if (
            strcmp(
                attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,
                oldName
            ) == 0
        )
        {
            attrToRenameRecId = attrCatRecId;
        }
    }


    // old attribute wasn't found
    if (attrToRenameRecId.block == -1 &&
        attrToRenameRecId.slot == -1)
    {
        return E_ATTRNOTEXIST;
    }


    //  Fetch old attribute's record again
    RecBuffer attrToRenameBuffer(
        attrToRenameRecId.block
    );

    int ret = attrToRenameBuffer.getRecord(
        attrCatEntryRecord,
        attrToRenameRecId.slot
    );

    if (ret != SUCCESS)
    {
        return ret;
    }


    //  Change oldName → newName
    strcpy(
        attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,
        newName
    );


    //  Write updated record
    ret = attrToRenameBuffer.setRecord(
        attrCatEntryRecord,
        attrToRenameRecId.slot
    );

    if (ret != SUCCESS)
    {
        return ret;
    }

    return SUCCESS;
}


int BlockAccess::insert(int relId, union Attribute *record)
{
    // --------------------------------------------------
    // 1. Get relation metadata from Relation Cache
    // --------------------------------------------------
    RelCatEntry relCatEntry;

    int ret = RelCacheTable::getRelCatEntry(
        relId,
        &relCatEntry
    );

    if (ret != SUCCESS)
    {
        return ret;
    }

    int blockNum = relCatEntry.firstBlk;
    int numOfSlots = relCatEntry.numSlotsPerBlk;
    int numOfAttributes = relCatEntry.numAttrs;

    RecId recId = {-1, -1};

    // Last block visited while traversing the linked list
    int prevBlockNum = -1;


    // --------------------------------------------------
    // 2. Search ALL existing blocks for a free slot
    // --------------------------------------------------
    while (blockNum != -1)
    {
        RecBuffer block(blockNum);

        HeadInfo head;

        ret = block.getHeader(&head);

        if (ret != SUCCESS)
        {
            return ret;
        }


        unsigned char slotMap[head.numSlots];

        ret = block.getSlotMap(slotMap);

        if (ret != SUCCESS)
        {
            return ret;
        }


        // Search this block for an empty slot
        for (int slot = 0; slot < head.numSlots; slot++)
        {
            if (slotMap[slot] == SLOT_UNOCCUPIED)
            {
                recId.block = blockNum;
                recId.slot = slot;
                break;
            }
        }


        // Free slot found
        if (recId.block != -1)
        {
            break;
        }


        // Move to next record block
        prevBlockNum = blockNum;
        blockNum = head.rblock;
    }


    // --------------------------------------------------
    // 3. No existing block has a free slot
    // --------------------------------------------------
    if (recId.block == -1)
    {
        // RELATIONCAT is not allowed to grow beyond its
        // allocated catalogue block(s)
        if (relId == RELCAT_RELID)
        {
            return E_MAXRELATIONS;
        }


        // Allocate a new record block
        RecBuffer newBlock;

        int newBlockNum = newBlock.getBlockNum();

        if (newBlockNum == E_DISKFULL)
        {
            return E_DISKFULL;
        }


        recId.block = newBlockNum;
        recId.slot = 0;


        // --------------------------------------------------
        // 4. Initialise new block header
        // --------------------------------------------------
        HeadInfo newHead;

        newHead.blockType = REC;

        // For record blocks, previous block is lblock
        newHead.pblock = -1;
        newHead.lblock = prevBlockNum;
        newHead.rblock = -1;

        newHead.numEntries = 0;
        newHead.numAttrs = numOfAttributes;
        newHead.numSlots = numOfSlots;


        ret = newBlock.setHeader(&newHead);

        if (ret != SUCCESS)
        {
            return ret;
        }


        // --------------------------------------------------
        // 5. Initialise new block slot map
        // --------------------------------------------------
        unsigned char newSlotMap[numOfSlots];

        for (int i = 0; i < numOfSlots; i++)
        {
            newSlotMap[i] = SLOT_UNOCCUPIED;
        }


        ret = newBlock.setSlotMap(newSlotMap);

        if (ret != SUCCESS)
        {
            return ret;
        }


        // --------------------------------------------------
        // 6. Link previous last block -> new block
        // --------------------------------------------------
        if (prevBlockNum != -1)
        {
            RecBuffer prevBlock(prevBlockNum);

            HeadInfo prevHead;

            ret = prevBlock.getHeader(&prevHead);

            if (ret != SUCCESS)
            {
                return ret;
            }

            prevHead.rblock = newBlockNum;

            ret = prevBlock.setHeader(&prevHead);

            if (ret != SUCCESS)
            {
                return ret;
            }
        }
        else
        {
            // This is the FIRST record block of the relation
            relCatEntry.firstBlk = newBlockNum;
        }


        // This new block is now the last block
        relCatEntry.lastBlk = newBlockNum;

        ret = RelCacheTable::setRelCatEntry(
            relId,
            &relCatEntry
        );

        if (ret != SUCCESS)
        {
            return ret;
        }
    }


    // --------------------------------------------------
    // 7. Insert the actual record
    // --------------------------------------------------
    RecBuffer targetBlock(recId.block);

    ret = targetBlock.setRecord(
        record,
        recId.slot
    );

    if (ret != SUCCESS)
    {
        return ret;
    }


    // --------------------------------------------------
    // 8. Mark inserted slot as occupied
    // --------------------------------------------------
    HeadInfo targetHead;

    ret = targetBlock.getHeader(&targetHead);

    if (ret != SUCCESS)
    {
        return ret;
    }


    unsigned char targetSlotMap[targetHead.numSlots];

    ret = targetBlock.getSlotMap(targetSlotMap);

    if (ret != SUCCESS)
    {
        return ret;
    }


    targetSlotMap[recId.slot] = SLOT_OCCUPIED;

    ret = targetBlock.setSlotMap(targetSlotMap);

    if (ret != SUCCESS)
    {
        return ret;
    }


    // --------------------------------------------------
    // 9. Increment number of entries in this block
    // --------------------------------------------------
    targetHead.numEntries++;

    ret = targetBlock.setHeader(&targetHead);

    if (ret != SUCCESS)
    {
        return ret;
    }


    // --------------------------------------------------
    // 10. Increment total number of relation records
    // --------------------------------------------------
    relCatEntry.numRecs++;

    ret = RelCacheTable::setRelCatEntry(
        relId,
        &relCatEntry
    );

    if (ret != SUCCESS)
    {
        return ret;
    }
    // -----------------------------------------
    // STAGE 11: Insert into existing B+ Trees
    // -----------------------------------------

    int flag = SUCCESS;

    // Get relation information
    RelCatEntry relEntry;

    ret = RelCacheTable::getRelCatEntry(
        relId, &relEntry
    );

    if (ret != SUCCESS)
        return ret;

    // Check every attribute
    for (int attrOffset = 0;
        attrOffset < relEntry.numAttrs;
        attrOffset++) {

        AttrCatEntry attrEntry;

        // Get attribute metadata
        ret = AttrCacheTable::getAttrCatEntry(
            relId, attrOffset, &attrEntry
        );

        if (ret != SUCCESS)
            return ret;

        // Skip attributes without indexes
        if (attrEntry.rootBlock == -1)
            continue;

        // Insert the record into the B+ Tree
        ret = BPlusTree::bPlusInsert(
            relId,
            attrEntry.attrName,
            record[attrOffset],
            recId
        );
        // The record was inserted, but its index
        // was removed because the disk became full
        if (ret == E_DISKFULL) {
            flag = E_INDEX_BLOCKS_RELEASED;
        }
        else if (ret != SUCCESS) {
            return ret;
        }
    }

    return flag;

}

int BlockAccess::search(
    int relId,
    Attribute *record,
    char *attrName,
    Attribute attrVal,
    int op
) {
    AttrCatEntry attrCatEntry;

    int ret = AttrCacheTable::getAttrCatEntry(
        relId, attrName, &attrCatEntry
    );

    if (ret != SUCCESS)
        return ret;

    RecId recId;

    if (attrCatEntry.rootBlock == -1) {

        recId = BlockAccess::linearSearch(
            relId, attrName, attrVal, op
        );

    } else {

        recId = BPlusTree::bPlusSearch(
            relId, attrName, attrVal, op
        );
    }

    if (recId.block == -1 && recId.slot == -1)
        return E_NOTFOUND;

    RecBuffer recBlock(recId.block);

    return recBlock.getRecord(record, recId.slot);
}



int BlockAccess::deleteRelation(char relName[ATTR_SIZE]) {

    // STEP 1: Protect system catalogs
    if (strcmp(relName, RELCAT_RELNAME) == 0 ||
        strcmp(relName, ATTRCAT_RELNAME) == 0) {
        return E_NOTPERMITTED;
    }

    // STEP 2: Find relation in RELATIONCAT
    Attribute relNameAttr{};
    strcpy(relNameAttr.sVal, relName);

    char relNameField[] = RELCAT_ATTR_RELNAME;

    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    RecId relRecId = BlockAccess::linearSearch(
        RELCAT_RELID,
        relNameField,
        relNameAttr,
        EQ
    );

    if (relRecId.block == -1) {
        return E_RELNOTEXIST;
    }

    // STEP 3: Read relation catalog record
    RecBuffer relCatBlock(relRecId.block);

    Attribute relRecord[RELCAT_NO_ATTRS];

    int ret = relCatBlock.getRecord(
        relRecord,
        relRecId.slot
    );

    if (ret != SUCCESS)
        return ret;


    // --------------------------------------------------
    // STEP 4: Release all relation data blocks
    // --------------------------------------------------

    int blockNum =
        (int)relRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;

    while (blockNum != -1) {

        RecBuffer dataBlock(blockNum);

        HeadInfo header;

        ret = dataBlock.getHeader(&header);

        if (ret != SUCCESS)
            return ret;

        int nextBlock = header.rblock;

        // Release current data block
        dataBlock.releaseBlock();

        // Move to next block
        blockNum = nextBlock;
    }


    // --------------------------------------------------
    // STEP 5: Delete ATTRIBUTECAT records
    // --------------------------------------------------

    char attrRelNameField[] = ATTRCAT_ATTR_RELNAME;

    int numberOfAttributesDeleted = 0;

    while (true) {

        // Reset search index before each search
        RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

        RecId attrRecId = BlockAccess::linearSearch(
            ATTRCAT_RELID,
            attrRelNameField,
            relNameAttr,
            EQ
        );

        // No more attributes belonging to this relation
        if (attrRecId.block == -1)
            break;

        // Open Attribute Catalog block
        RecBuffer attrBlock(attrRecId.block);

        HeadInfo header;

        ret = attrBlock.getHeader(&header);

        if (ret != SUCCESS)
            return ret;


        // ==============================================
        // STAGE 11: DESTROY B+ TREE INDEX
        // ==============================================

        // Read attribute catalog record
        Attribute attrRecord[ATTRCAT_NO_ATTRS];

        ret = attrBlock.getRecord(
            attrRecord,
            attrRecId.slot
        );

        if (ret != SUCCESS)
            return ret;

        // Obtain B+ Tree root block
        int rootBlock =
            (int)attrRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal;

        // Delete index if one exists
        if (rootBlock != -1) {

            ret = BPlusTree::bPlusDestroy(rootBlock);

            if (ret != SUCCESS)
                return ret;
        }

        // ==============================================
        // END OF STAGE 11 ADDITION
        // ==============================================


        // --------------------------------------------------
        // Mark attribute record as unoccupied
        // --------------------------------------------------

        unsigned char slotMap[BLOCK_SIZE];

        ret = attrBlock.getSlotMap(slotMap);

        if (ret != SUCCESS)
            return ret;

        slotMap[attrRecId.slot] = SLOT_UNOCCUPIED;

        ret = attrBlock.setSlotMap(slotMap);

        if (ret != SUCCESS)
            return ret;

        // Decrease occupied entries
        header.numEntries--;

        ret = attrBlock.setHeader(&header);

        if (ret != SUCCESS)
            return ret;


        // --------------------------------------------------
        // STEP 6: Release empty ATTRIBUTECAT blocks
        // --------------------------------------------------

        if (header.numEntries == 0) {

            // Update previous block
            if (header.lblock != -1) {

                RecBuffer leftBlock(header.lblock);

                HeadInfo leftHeader;

                ret = leftBlock.getHeader(&leftHeader);

                if (ret != SUCCESS)
                    return ret;

                leftHeader.rblock = header.rblock;

                ret = leftBlock.setHeader(&leftHeader);

                if (ret != SUCCESS)
                    return ret;
            }

            // Update next block
            if (header.rblock != -1) {

                RecBuffer rightBlock(header.rblock);

                HeadInfo rightHeader;

                ret = rightBlock.getHeader(&rightHeader);

                if (ret != SUCCESS)
                    return ret;

                rightHeader.lblock = header.lblock;

                ret = rightBlock.setHeader(&rightHeader);

                if (ret != SUCCESS)
                    return ret;

            } else {

                // Released block was the final block
                RelCatEntry attrMeta;

                ret = RelCacheTable::getRelCatEntry(
                    ATTRCAT_RELID,
                    &attrMeta
                );

                if (ret != SUCCESS)
                    return ret;

                attrMeta.lastBlk = header.lblock;

                ret = RelCacheTable::setRelCatEntry(
                    ATTRCAT_RELID,
                    &attrMeta
                );

                if (ret != SUCCESS)
                    return ret;
            }

            // Release empty Attribute Catalog block
            attrBlock.releaseBlock();
        }

        numberOfAttributesDeleted++;
    }


    // --------------------------------------------------
    // STEP 7: Delete RELATIONCAT entry
    // --------------------------------------------------

    HeadInfo relHeader;

    ret = relCatBlock.getHeader(&relHeader);

    if (ret != SUCCESS)
        return ret;

    unsigned char relSlotMap[BLOCK_SIZE];

    ret = relCatBlock.getSlotMap(relSlotMap);

    if (ret != SUCCESS)
        return ret;

    // Mark relation catalog record as free
    relSlotMap[relRecId.slot] = SLOT_UNOCCUPIED;

    ret = relCatBlock.setSlotMap(relSlotMap);

    if (ret != SUCCESS)
        return ret;

    // Update number of records in this block
    relHeader.numEntries--;

    ret = relCatBlock.setHeader(&relHeader);

    if (ret != SUCCESS)
        return ret;


    // --------------------------------------------------
    // STEP 8: Update RELATIONCAT cache
    // --------------------------------------------------

    RelCatEntry relMeta;

    ret = RelCacheTable::getRelCatEntry(
        RELCAT_RELID,
        &relMeta
    );

    if (ret != SUCCESS)
        return ret;

    relMeta.numRecs--;

    ret = RelCacheTable::setRelCatEntry(
        RELCAT_RELID,
        &relMeta
    );

    if (ret != SUCCESS)
        return ret;


    // --------------------------------------------------
    // STEP 9: Update ATTRIBUTECAT cache
    // --------------------------------------------------

    RelCatEntry attrMeta;

    ret = RelCacheTable::getRelCatEntry(
        ATTRCAT_RELID,
        &attrMeta
    );

    if (ret != SUCCESS)
        return ret;

    attrMeta.numRecs -= numberOfAttributesDeleted;

    ret = RelCacheTable::setRelCatEntry(
        ATTRCAT_RELID,
        &attrMeta
    );

    if (ret != SUCCESS)
        return ret;

    return SUCCESS;
}



int BlockAccess::project(int relId, Attribute *record) {

    // STEP 1: Get previous search index
    RecId prevRecId;

    int ret = RelCacheTable::getSearchIndex(
        relId, &prevRecId
    );

    if (ret != SUCCESS)
        return ret;

    int block, slot;

    // STEP 2: Determine starting position
    if (prevRecId.block == -1 && prevRecId.slot == -1) {

        RelCatEntry relCatEntry;

        ret = RelCacheTable::getRelCatEntry(
            relId, &relCatEntry
        );

        if (ret != SUCCESS)
            return ret;

        block = relCatEntry.firstBlk;
        slot = 0;

    } else {

        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }

    // STEP 3: Find next occupied slot
    while (block != -1) {

        RecBuffer recBlock(block);

        HeadInfo header;

        ret = recBlock.getHeader(&header);

        if (ret != SUCCESS)
            return ret;

        // Move to next block if current block is exhausted
        if (slot >= header.numSlots) {
            block = header.rblock;
            slot = 0;
            continue;
        }

        unsigned char slotMap[BLOCK_SIZE];

        ret = recBlock.getSlotMap(slotMap);

        if (ret != SUCCESS)
            return ret;

        // Skip unoccupied slots
        if (slotMap[slot] == SLOT_UNOCCUPIED) {
            slot++;
            continue;
        }

        // STEP 4: Copy the record
        ret = recBlock.getRecord(record, slot);

        if (ret != SUCCESS)
            return ret;

        // STEP 5: Update search index
        RecId nextRecId = {block, slot};

        return RelCacheTable::setSearchIndex(
            relId, &nextRecId
        );
    }

    // No more records
    return E_NOTFOUND;
}


