#include "AttrCacheTable.h"

#include <cstring>

AttrCacheEntry *AttrCacheTable::attrCache[MAX_OPEN];

int AttrCacheTable::getAttrCatEntry(
    int relId,
    int attrOffset,
    AttrCatEntry *attrCatBuf)
{
    if (relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }

    if (attrCache[relId] == nullptr)
    {
        return E_RELNOTOPEN;
    }

    for (AttrCacheEntry *entry = attrCache[relId];
         entry != nullptr;
         entry = entry->next)
    {
        if (entry->attrCatEntry.offset == attrOffset)
        {
            *attrCatBuf = entry->attrCatEntry;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}
//overloaded function to get attribute catalog entry by attribute name
int AttrCacheTable::getAttrCatEntry(
    int relId,
    char attrName[ATTR_SIZE],
    AttrCatEntry *attrCatBuf)
{
    if (relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }

    if (attrCache[relId] == nullptr)
    {
        return E_RELNOTOPEN;
    }

    for (AttrCacheEntry *entry = attrCache[relId];
         entry != nullptr;
         entry = entry->next)
    {
        if (strcmp(entry->attrCatEntry.attrName, attrName) == 0)
        {
            *attrCatBuf = entry->attrCatEntry;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

void AttrCacheTable::recordToAttrCatEntry(
    union Attribute record[ATTRCAT_NO_ATTRS],
    AttrCatEntry *attrCatEntry)
{
    strcpy(
        attrCatEntry->relName,
        record[ATTRCAT_REL_NAME_INDEX].sVal
    );

    strcpy(
        attrCatEntry->attrName,
        record[ATTRCAT_ATTR_NAME_INDEX].sVal
    );

    attrCatEntry->attrType =
        (int)record[ATTRCAT_ATTR_TYPE_INDEX].nVal;

    attrCatEntry->primaryFlag =
        (int)record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal;

    attrCatEntry->rootBlock =
        (int)record[ATTRCAT_ROOT_BLOCK_INDEX].nVal;

    attrCatEntry->offset =
        (int)record[ATTRCAT_OFFSET_INDEX].nVal;
}


int AttrCacheTable::getSearchIndex(
    int relId,
    char attrName[ATTR_SIZE],
    IndexId *searchIndex
) {
    // Step 1: Validate relation ID
    if (relId < 0 || relId >= MAX_OPEN)
        return E_OUTOFBOUND;

    // Step 2: Check whether relation is open
    if (attrCache[relId] == nullptr)
        return E_RELNOTOPEN;

    // Step 3: Start from first attribute
    AttrCacheEntry *entry = attrCache[relId];

    // Step 4: Search for the attribute
    while (entry != nullptr) {

        if (strcmp(entry->attrCatEntry.attrName,
                   attrName) == 0) {

            // Step 5: Copy the search position
            *searchIndex = entry->searchIndex;

            return SUCCESS;
        }

        entry = entry->next;
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::getSearchIndex(
    int relId,
    int attrOffset,
    IndexId *searchIndex
) {
    if (relId < 0 || relId >= MAX_OPEN)
        return E_OUTOFBOUND;

    if (attrCache[relId] == nullptr)
        return E_RELNOTOPEN;

    AttrCacheEntry *entry = attrCache[relId];

    while (entry != nullptr) {

        if (entry->attrCatEntry.offset == attrOffset) {

            *searchIndex = entry->searchIndex;

            return SUCCESS;
        }

        entry = entry->next;
    }

    return E_ATTRNOTEXIST;
}


int AttrCacheTable::setSearchIndex(
    int relId,
    char attrName[ATTR_SIZE],
    IndexId *searchIndex
) {
    // Check valid relation ID
    if (relId < 0 || relId >= MAX_OPEN)
        return E_OUTOFBOUND;

    // Check relation is open
    if (attrCache[relId] == nullptr)
        return E_RELNOTOPEN;

    // Start from first attribute
    AttrCacheEntry *entry = attrCache[relId];

    // Search for matching attribute
    while (entry != nullptr) {

        if (strcmp(entry->attrCatEntry.attrName,
                   attrName) == 0) {

            entry->searchIndex = *searchIndex;

            return SUCCESS;
        }

        entry = entry->next;
    }

    return E_ATTRNOTEXIST;
}


int AttrCacheTable::setSearchIndex(
    int relId,
    int attrOffset,
    IndexId *searchIndex
) {
    if (relId < 0 || relId >= MAX_OPEN)
        return E_OUTOFBOUND;

    if (attrCache[relId] == nullptr)
        return E_RELNOTOPEN;

    AttrCacheEntry *entry = attrCache[relId];

    while (entry != nullptr) {

        if (entry->attrCatEntry.offset == attrOffset) {

            entry->searchIndex = *searchIndex;

            return SUCCESS;
        }

        entry = entry->next;
    }

    return E_ATTRNOTEXIST;
}


int AttrCacheTable::resetSearchIndex(
    int relId,
    char attrName[ATTR_SIZE]
) {
    IndexId searchIndex = {-1, -1};

    return AttrCacheTable::setSearchIndex(
        relId, attrName, &searchIndex
    );
}


int AttrCacheTable::resetSearchIndex(
    int relId,
    int attrOffset
) {
    IndexId searchIndex = {-1, -1};

    return AttrCacheTable::setSearchIndex(
        relId, attrOffset, &searchIndex
    );
}