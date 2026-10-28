#include "BPlusTree.h"
#include <vector>
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


int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE]) {

    // Do not create indexes on system catalogs
    if (relId == RELCAT_RELID || relId == ATTRCAT_RELID)
        return E_NOTPERMITTED;

    // Get attribute information
    AttrCatEntry attr;

    int ret = AttrCacheTable::getAttrCatEntry(
        relId, attrName, &attr
    );

    if (ret != SUCCESS)
        return ret;

    // Index already exists
    if (attr.rootBlock != -1)
        return SUCCESS;

    // Create an empty leaf block as the root
    IndLeaf newRoot;

    int root = newRoot.getBlockNum();

    if (root < 0)
        return root;

    // Initialize root header
    HeadInfo rootHead{};

    rootHead.blockType = IND_LEAF;
    rootHead.pblock = -1;
    rootHead.lblock = -1;
    rootHead.rblock = -1;
    rootHead.numEntries = 0;

    ret = newRoot.setHeader(&rootHead);

    if (ret != SUCCESS) {
        newRoot.releaseBlock();
        return ret;
    }

    // Save root block in Attribute Cache
    attr.rootBlock = root;

    ret = AttrCacheTable::setAttrCatEntry(
        relId, attrName, &attr
    );

    if (ret != SUCCESS) {
        newRoot.releaseBlock();
        return ret;
    }

    // Clean up the partially created index on errors
    auto cleanup = [&](int error) {

        AttrCatEntry latest;

        if (AttrCacheTable::getAttrCatEntry(
                relId, attrName, &latest) == SUCCESS) {

            if (latest.rootBlock != -1) {

                bPlusDestroy(latest.rootBlock);

                latest.rootBlock = -1;

                AttrCacheTable::setAttrCatEntry(
                    relId, attrName, &latest
                );
            }
        }

        return error;
    };

    // Get relation information
    RelCatEntry relation;

    ret = RelCacheTable::getRelCatEntry(
        relId, &relation
    );

    if (ret != SUCCESS)
        return cleanup(ret);

    if (relation.numAttrs <= 0 ||
        relation.numSlotsPerBlk <= 0)
        return cleanup(E_INVALID);

    // Temporary storage for slot map and records
    std::vector<unsigned char> slotMap(
        relation.numSlotsPerBlk
    );

    std::vector<Attribute> record(
        relation.numAttrs
    );

    // Start from the first record block
    int block = relation.firstBlk;

    while (block != -1) {

        RecBuffer records(block);

        ret = records.getSlotMap(slotMap.data());

        if (ret != SUCCESS)
            return cleanup(ret);

        // Visit every occupied slot
        for (int slot = 0;
             slot < relation.numSlotsPerBlk;
             slot++) {

            if (slotMap[slot] != SLOT_OCCUPIED)
                continue;

            // Read the record
            ret = records.getRecord(
                record.data(), slot
            );

            if (ret != SUCCESS)
                return cleanup(ret);

            // Actual record location
            RecId recordId = {block, slot};

            // Insert indexed attribute into B+ Tree
            ret = bPlusInsert(
                relId,
                attrName,
                record[attr.offset],
                recordId
            );

            if (ret != SUCCESS)
                return cleanup(ret);
        }

        // Move to the next record block
        HeadInfo head;

        ret = records.getHeader(&head);

        if (ret != SUCCESS)
            return cleanup(ret);

        block = head.rblock;
    }

    return SUCCESS;
}


int BPlusTree::bPlusDestroy(int rootBlockNum) {

    // Validate block number
    if (rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS)
        return E_OUTOFBOUND;

    // Find block type
    int type = StaticBuffer::getStaticBlockType(
        rootBlockNum
    );

    // Leaf block: release directly
    if (type == IND_LEAF) {

        IndLeaf leaf(rootBlockNum);

        leaf.releaseBlock();

        return SUCCESS;
    }

    // Reject non-index blocks
    if (type != IND_INTERNAL)
        return E_INVALIDBLOCK;

    // Open internal index block
    IndInternal node(rootBlockNum);

    HeadInfo head;

    int ret = node.getHeader(&head);

    if (ret != SUCCESS)
        return ret;

    if (head.numEntries < 1 ||
        head.numEntries > MAX_KEYS_INTERNAL)
        return E_INVALIDBLOCK;

    // Recursively delete child blocks
    for (int i = 0; i < head.numEntries; i++) {

        InternalEntry entry;

        ret = node.getEntry(&entry, i);

        if (ret != SUCCESS)
            return ret;

        // Delete first left child only once
        if (i == 0) {

            ret = bPlusDestroy(entry.lChild);

            if (ret != SUCCESS)
                return ret;
        }

        // Delete each right child
        ret = bPlusDestroy(entry.rChild);

        if (ret != SUCCESS)
            return ret;
    }

    // Finally delete current internal block
    node.releaseBlock();

    return SUCCESS;
}


int BPlusTree::findLeafToInsert(
    int rootBlock, Attribute attrVal, int attrType) {

    // Start from the root
    int blockNum = rootBlock;

    // Traverse internal nodes until we reach a leaf
    while (StaticBuffer::getStaticBlockType(blockNum) == IND_INTERNAL) {

        IndInternal node(blockNum);
        HeadInfo header;

        int ret = node.getHeader(&header);
        if (ret != SUCCESS)
            return ret;

        if (header.numEntries < 1 ||
            header.numEntries > MAX_KEYS_INTERNAL)
            return E_INVALIDBLOCK;

        // Find the correct child
        for (int i = 0; i < header.numEntries; i++) {

            InternalEntry entry;

            ret = node.getEntry(&entry, i);
            if (ret != SUCCESS)
                return ret;

            // Go left when the new value <= separator key
            if (compareAttrs(entry.attrVal, attrVal, attrType) >= 0) {
                blockNum = entry.lChild;
                break;
            }

            // Otherwise, choose the rightmost child if needed
            if (i == header.numEntries - 1) {
                blockNum = entry.rChild;
            }
        }
    }

    // Verify that we reached a leaf
    if (blockNum < 0 ||
        StaticBuffer::getStaticBlockType(blockNum) != IND_LEAF)
        return E_INVALIDBLOCK;

    return blockNum;
}


int BPlusTree::bPlusInsert(
    int relId,
    char attrName[ATTR_SIZE],
    Attribute attrVal,
    RecId recordId
) {

    // Get attribute information
    AttrCatEntry attr;

    int ret = AttrCacheTable::getAttrCatEntry(
        relId, attrName, &attr
    );

    if (ret != SUCCESS)
        return ret;

    // Check whether index exists
    if (attr.rootBlock == -1)
        return E_NOINDEX;

    // Find correct leaf block
    int leafBlock = findLeafToInsert(
        attr.rootBlock,
        attrVal,
        attr.attrType
    );

    if (leafBlock < 0)
        return leafBlock;

    // Create a leaf index entry
    Index indexEntry{};

    indexEntry.attrVal = attrVal;
    indexEntry.block = recordId.block;
    indexEntry.slot = recordId.slot;

    // Insert into leaf
    ret = insertIntoLeaf(
        relId,
        attrName,
        leafBlock,
        indexEntry
    );

    // Handle insufficient disk space
    if (ret == E_DISKFULL) {

        // Retrieve latest root (it may have changed)
        AttrCatEntry latest;

        int status = AttrCacheTable::getAttrCatEntry(
            relId, attrName, &latest
        );

        if (status != SUCCESS)
            return status;

        // Delete the incomplete B+ Tree
        if (latest.rootBlock != -1) {

            bPlusDestroy(latest.rootBlock);

            latest.rootBlock = -1;

            AttrCacheTable::setAttrCatEntry(
                relId, attrName, &latest
            );
        }

        return E_DISKFULL;
    }

    return ret;
}

int BPlusTree::insertIntoLeaf(
    int relId,
    char attrName[ATTR_SIZE],
    int blockNum,
    Index indexEntry
) {
    // Get attribute information
    AttrCatEntry attr;

    int ret = AttrCacheTable::getAttrCatEntry(
        relId, attrName, &attr
    );

    if (ret != SUCCESS)
        return ret;

    // Open the leaf block
    IndLeaf leaf(blockNum);
    HeadInfo head;

    ret = leaf.getHeader(&head);

    if (ret != SUCCESS)
        return ret;

    if (head.numEntries < 0 ||
        head.numEntries > MAX_KEYS_LEAF)
        return E_INVALIDBLOCK;

    // Temporary array for existing entries + new entry
    Index indices[MAX_KEYS_LEAF + 1];

    int count = head.numEntries;
    int pos = 0;
    bool inserted = false;

    // Insert entries in sorted order
    for (int i = 0; i < count; i++) {

        Index current;

        ret = leaf.getEntry(&current, i);

        if (ret != SUCCESS)
            return ret;

        // Insert new entry before the first greater key
        if (!inserted &&
            compareAttrs(
                current.attrVal,
                indexEntry.attrVal,
                attr.attrType
            ) > 0) {

            indices[pos++] = indexEntry;
            inserted = true;
        }

        indices[pos++] = current;
    }

    // Insert at the end if needed
    if (!inserted)
        indices[pos++] = indexEntry;

    // Case 1: Leaf has space
    if (count < MAX_KEYS_LEAF) {

        for (int i = 0; i < pos; i++) {

            ret = leaf.setEntry(&indices[i], i);

            if (ret != SUCCESS)
                return ret;
        }

        head.numEntries = pos;

        return leaf.setHeader(&head);
    }

    // Case 2: Leaf is full, so split it
    int rightBlock = splitLeaf(blockNum, indices);

    if (rightBlock < 0)
        return rightBlock;

    // Separator = largest key in the left leaf
    Attribute middleValue =
        indices[MIDDLE_INDEX_LEAF].attrVal;

    // If the current leaf has a parent
    if (head.pblock != -1) {

        InternalEntry newEntry{};

        newEntry.lChild = blockNum;
        newEntry.attrVal = middleValue;
        newEntry.rChild = rightBlock;

        return insertIntoInternal(
            relId,
            attrName,
            head.pblock,
            newEntry
        );
    }

    // Otherwise create a new root
    return createNewRoot(
        relId,
        attrName,
        middleValue,
        blockNum,
        rightBlock
    );
}


int BPlusTree::splitLeaf(
    int leafBlockNum,
    Index indices[]
) {
    // Open existing leaf (left block)
    IndLeaf leftLeaf(leafBlockNum);

    HeadInfo leftHead;

    int ret = leftLeaf.getHeader(&leftHead);

    if (ret != SUCCESS)
        return ret;

    // Allocate new right leaf
    IndLeaf rightLeaf;

    int rightBlockNum = rightLeaf.getBlockNum();

    if (rightBlockNum < 0)
        return rightBlockNum;

    // Remember the original next leaf
    int oldRightBlock = leftHead.rblock;

    // Prepare right leaf header
    HeadInfo rightHead = leftHead;

    rightHead.blockType = IND_LEAF;
    rightHead.numEntries = (MAX_KEYS_LEAF + 1) / 2;
    rightHead.pblock = leftHead.pblock;
    rightHead.lblock = leafBlockNum;
    rightHead.rblock = oldRightBlock;

    // Prepare left leaf header
    leftHead.numEntries = (MAX_KEYS_LEAF + 1) / 2;
    leftHead.rblock = rightBlockNum;

    // Write first 32 entries to left leaf
    for (int i = 0; i <= MIDDLE_INDEX_LEAF; i++) {

        ret = leftLeaf.setEntry(&indices[i], i);

        if (ret != SUCCESS)
            return ret;
    }

    // Write remaining 32 entries to right leaf
    for (int i = MIDDLE_INDEX_LEAF + 1;
         i <= MAX_KEYS_LEAF;
         i++) {

        int rightIndex = i - (MIDDLE_INDEX_LEAF + 1);

        ret = rightLeaf.setEntry(
            &indices[i], rightIndex
        );

        if (ret != SUCCESS)
            return ret;
    }

    // Update both block headers
    ret = rightLeaf.setHeader(&rightHead);

    if (ret != SUCCESS)
        return ret;

    ret = leftLeaf.setHeader(&leftHead);

    if (ret != SUCCESS)
        return ret;

    // Update backward pointer of the original next leaf
    if (oldRightBlock != -1) {

        IndLeaf nextLeaf(oldRightBlock);
        HeadInfo nextHead;

        ret = nextLeaf.getHeader(&nextHead);

        if (ret != SUCCESS)
            return ret;

        nextHead.lblock = rightBlockNum;

        ret = nextLeaf.setHeader(&nextHead);

        if (ret != SUCCESS)
            return ret;
    }

    // Return newly created right block
    return rightBlockNum;
}


int BPlusTree::insertIntoInternal(
    int relId,
    char attrName[ATTR_SIZE],
    int intBlockNum,
    InternalEntry intEntry
) {
    // Get attribute information
    AttrCatEntry attr;

    int ret = AttrCacheTable::getAttrCatEntry(
        relId, attrName, &attr
    );

    if (ret != SUCCESS)
        return ret;

    // Open internal block
    IndInternal node(intBlockNum);
    HeadInfo head;

    ret = node.getHeader(&head);

    if (ret != SUCCESS)
        return ret;

    if (head.numEntries < 1 ||
        head.numEntries > MAX_KEYS_INTERNAL)
        return E_INVALIDBLOCK;

    int n = head.numEntries;

    InternalEntry oldEntries[MAX_KEYS_INTERNAL];
    InternalEntry entries[MAX_KEYS_INTERNAL + 1];

    // Read existing entries
    for (int i = 0; i < n; i++) {

        ret = node.getEntry(&oldEntries[i], i);

        if (ret != SUCCESS)
            return ret;
    }

    // Find the child that was split
    int pos = -1;

    for (int i = 0; i < n; i++) {

        if (oldEntries[i].lChild == intEntry.lChild) {
            pos = i;
            break;
        }
    }

    if (pos == -1 &&
        oldEntries[n - 1].rChild == intEntry.lChild) {
        pos = n;
    }

    // Find position using key if needed
    if (pos == -1) {

        pos = n;

        for (int i = 0; i < n; i++) {

            if (compareAttrs(
                    oldEntries[i].attrVal,
                    intEntry.attrVal,
                    attr.attrType
                ) > 0) {

                pos = i;
                break;
            }
        }
    }

    // Copy entries before insertion position
    for (int i = 0; i < pos; i++)
        entries[i] = oldEntries[i];

    // Insert the entry produced by the child split
    entries[pos] = intEntry;

    // Shift existing entries to the right
    for (int i = pos; i < n; i++) {
        entries[i + 1] = oldEntries[i];
    }

    // The new right child replaces the old child
    // in the next entry
    if (pos < n) {
        entries[pos + 1].lChild = intEntry.rChild;
    }

    // Maintain consistent child pointers
    for (int i = 0; i < n; i++) {
        entries[i].rChild = entries[i + 1].lChild;
    }
    // Case 1: Internal node has space
    if (n < MAX_KEYS_INTERNAL) {

        for (int i = 0; i <= n; i++) {

            ret = node.setEntry(&entries[i], i);

            if (ret != SUCCESS)
                return ret;
        }

        head.numEntries = n + 1;

        return node.setHeader(&head);
    }

    // Case 2: Internal node is full
    int rightBlock = splitInternal(
        intBlockNum, entries
    );

    if (rightBlock < 0) {

        if (rightBlock == E_DISKFULL)
            bPlusDestroy(intEntry.rChild);

        return rightBlock;
    }

    // Promote middle key to parent
    InternalEntry promoted{};

    promoted.lChild = intBlockNum;

    promoted.attrVal =
        entries[MIDDLE_INDEX_INTERNAL].attrVal;

    promoted.rChild = rightBlock;

    // If parent exists, insert recursively
    if (head.pblock != -1) {

        return insertIntoInternal(
            relId,
            attrName,
            head.pblock,
            promoted
        );
    }

    // Otherwise create a new root
    return createNewRoot(
        relId,
        attrName,
        promoted.attrVal,
        promoted.lChild,
        promoted.rChild
    );
}



int BPlusTree::splitInternal(
    int intBlockNum,
    InternalEntry internalEntries[]
) {
    // Open existing internal block
    IndInternal left(intBlockNum);

    HeadInfo leftHead;

    int ret = left.getHeader(&leftHead);

    if (ret != SUCCESS)
        return ret;

    // Allocate new internal block
    IndInternal right;

    int rightBlock = right.getBlockNum();

    if (rightBlock < 0)
        return rightBlock;

    // Prepare headers
    HeadInfo rightHead = leftHead;

    rightHead.blockType = IND_INTERNAL;

    rightHead.numEntries =
        MAX_KEYS_INTERNAL / 2;

    rightHead.lblock = -1;
    rightHead.rblock = -1;

    leftHead.numEntries =
        MAX_KEYS_INTERNAL / 2;

    // Distribute entries between blocks
    for (int i = 0; i < MAX_KEYS_INTERNAL / 2; i++) {

        // First 50 entries remain in left
        ret = left.setEntry(&internalEntries[i], i);

        if (ret != SUCCESS)
            return ret;

        // Last 50 entries move to right
        ret = right.setEntry(
            &internalEntries[i + MIDDLE_INDEX_INTERNAL + 1],
            i
        );

        if (ret != SUCCESS)
            return ret;
    }

    // Update headers
    ret = left.setHeader(&leftHead);

    if (ret != SUCCESS)
        return ret;

    ret = right.setHeader(&rightHead);

    if (ret != SUCCESS)
        return ret;

    // Update parent pointers of moved children
    for (int i = MIDDLE_INDEX_INTERNAL + 1;
         i <= MAX_KEYS_INTERNAL;
         i++) {

        BlockBuffer child(internalEntries[i].lChild);

        HeadInfo childHead;

        ret = child.getHeader(&childHead);

        if (ret != SUCCESS)
            return ret;

        childHead.pblock = rightBlock;

        ret = child.setHeader(&childHead);

        if (ret != SUCCESS)
            return ret;
    }

    // Update rightmost child's parent
    BlockBuffer lastChild(
        internalEntries[MAX_KEYS_INTERNAL].rChild
    );

    HeadInfo lastHead;

    ret = lastChild.getHeader(&lastHead);

    if (ret != SUCCESS)
        return ret;

    lastHead.pblock = rightBlock;

    ret = lastChild.setHeader(&lastHead);

    if (ret != SUCCESS)
        return ret;

    return rightBlock;
}


int BPlusTree::createNewRoot(
    int relId,
    char attrName[ATTR_SIZE],
    Attribute attrVal,
    int lChild,
    int rChild
) {
    // Get attribute information
    AttrCatEntry attr;

    int ret = AttrCacheTable::getAttrCatEntry(
        relId, attrName, &attr
    );

    if (ret != SUCCESS)
        return ret;

    // Allocate new root block
    IndInternal root;

    int newRootBlock = root.getBlockNum();

    if (newRootBlock < 0) {

        if (newRootBlock == E_DISKFULL)
            bPlusDestroy(rChild);

        return newRootBlock;
    }

    // Initialize root header
    HeadInfo rootHead{};

    rootHead.blockType = IND_INTERNAL;
    rootHead.pblock = -1;
    rootHead.lblock = -1;
    rootHead.rblock = -1;
    rootHead.numEntries = 1;

    ret = root.setHeader(&rootHead);

    if (ret != SUCCESS)
        return ret;

    // Create root entry
    InternalEntry entry{};

    entry.lChild = lChild;
    entry.attrVal = attrVal;
    entry.rChild = rChild;

    ret = root.setEntry(&entry, 0);

    if (ret != SUCCESS)
        return ret;

    // Update parent pointers of children
    BlockBuffer left(lChild);
    BlockBuffer right(rChild);

    HeadInfo childHead;

    ret = left.getHeader(&childHead);

    if (ret != SUCCESS)
        return ret;

    childHead.pblock = newRootBlock;

    ret = left.setHeader(&childHead);

    if (ret != SUCCESS)
        return ret;

    ret = right.getHeader(&childHead);

    if (ret != SUCCESS)
        return ret;

    childHead.pblock = newRootBlock;

    ret = right.setHeader(&childHead);

    if (ret != SUCCESS)
        return ret;

    // Update root block in Attribute Cache
    attr.rootBlock = newRootBlock;

    return AttrCacheTable::setAttrCatEntry(
        relId, attrName, &attr
    );
}