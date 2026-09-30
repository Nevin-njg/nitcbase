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