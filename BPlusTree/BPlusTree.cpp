#include "BPlusTree.h"

#include <cstring>


long long stage10BPlusComparisons = 0;

RecId BPlusTree::bPlusSearch(
    int relId, char attrName[ATTR_SIZE],
    Attribute attrVal, int op) {

    RecId invalid = {-1, -1};

    IndexId searchIndex;
    AttrCatEntry attrCatEntry;

    if (AttrCacheTable::getSearchIndex(
            relId, attrName, &searchIndex) != SUCCESS)
        return invalid;

    if (AttrCacheTable::getAttrCatEntry(
            relId, attrName, &attrCatEntry) != SUCCESS)
        return invalid;

    if (attrCatEntry.rootBlock == -1)
        return invalid;

    int block, index;

    if (searchIndex.block == -1 && searchIndex.index == -1) {
        block = attrCatEntry.rootBlock;
        index = 0;
    }
    else {
        block = searchIndex.block;
        index = searchIndex.index + 1;

        IndLeaf leaf(block);
        HeadInfo head;

        if (leaf.getHeader(&head) != SUCCESS)
            return invalid;

        if (index >= head.numEntries) {
            block = head.rblock;
            index = 0;
        }
    }

    // Traverse internal nodes
    while (block != -1 &&
           StaticBuffer::getStaticBlockType(block) == IND_INTERNAL) {

        IndInternal internal(block);
        HeadInfo head;

        if (internal.getHeader(&head) != SUCCESS)
            return invalid;

        if (head.numEntries < 1 ||
            head.numEntries > MAX_KEYS_INTERNAL)
            return invalid;

        InternalEntry entry;

        if (op == NE || op == LT || op == LE) {

            if (internal.getEntry(&entry, 0) != SUCCESS)
                return invalid;

            block = entry.lChild;
        }
        else {
            bool found = false;

            for (int i = 0; i < head.numEntries; i++) {

                if (internal.getEntry(&entry, i) != SUCCESS)
                    return invalid;

                int cmp = compareAttrs(
                    entry.attrVal,
                    attrVal,
                    attrCatEntry.attrType
                );

                if ((op == GT && cmp > 0) ||
                    ((op == EQ || op == GE) && cmp >= 0)) {

                    block = entry.lChild;
                    found = true;
                    break;
                }
            }

            if (!found)
                block = entry.rChild;
        }
    }

    // Search leaf nodes
    while (block != -1) {

        if (StaticBuffer::getStaticBlockType(block) != IND_LEAF)
            return invalid;

        IndLeaf leaf(block);
        HeadInfo head;

        if (leaf.getHeader(&head) != SUCCESS)
            return invalid;

        if (head.numEntries < 0 ||
            head.numEntries > MAX_KEYS_LEAF)
            return invalid;

        for (; index < head.numEntries; index++) {

            Index entry;

            if (leaf.getEntry(&entry, index) != SUCCESS)
                return invalid;

            stage10BPlusComparisons++;    

            int cmp = compareAttrs(
                entry.attrVal,
                attrVal,
                attrCatEntry.attrType
            );

            bool match =
                (op == EQ && cmp == 0) ||
                (op == LT && cmp < 0) ||
                (op == LE && cmp <= 0) ||
                (op == GT && cmp > 0) ||
                (op == GE && cmp >= 0) ||
                (op == NE && cmp != 0);

            if (match) {

                IndexId current = {block, index};

                if (AttrCacheTable::setSearchIndex(
                        relId, attrName, &current) != SUCCESS)
                    return invalid;

                RecId recId = {entry.block, entry.slot};

                return recId;
            }

            if ((op == EQ || op == LT || op == LE) && cmp > 0)
                return invalid;
        }

        block = head.rblock;
        index = 0;
    }

    return invalid;
}