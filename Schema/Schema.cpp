#include "Schema.h"

#include <cmath>
#include <cstring>


int Schema::openRel(char relName[ATTR_SIZE]) {
  int ret = OpenRelTable::openRel(relName);

  // OpenRelTable::openRel() returns the rel-id on success.
  // Valid rel-ids are non-negative; error codes are negative.
  if (ret >= 0) {
    return SUCCESS;
  }

  return ret;
}

int Schema::closeRel(char relName[ATTR_SIZE]) {
  // RELATIONCAT and ATTRIBUTECAT must always remain open.
  if (strcmp(relName, RELCAT_RELNAME) == 0 ||
      strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  // Get the rel-id if the relation is currently open.
  int relId = OpenRelTable::getRelId(relName);

  if (relId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  return OpenRelTable::closeRel(relId);
}

int Schema::renameRel(
    char oldRelName[ATTR_SIZE],
    char newRelName[ATTR_SIZE]
) {

    // System catalogs cannot be renamed or used as a new relation name
    if (
        strcmp(oldRelName, RELCAT_RELNAME) == 0 ||
        strcmp(oldRelName, ATTRCAT_RELNAME) == 0 ||
        strcmp(newRelName, RELCAT_RELNAME) == 0 ||
        strcmp(newRelName, ATTRCAT_RELNAME) == 0
    ) {
        return E_NOTPERMITTED;
    }

    // Schema can be modified only when the relation is closed
    if (OpenRelTable::getRelId(oldRelName) != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    // Actual catalog modification is handled by BlockAccess
    return BlockAccess::renameRelation(oldRelName, newRelName);
}

int Schema::renameAttr(
    char relName[ATTR_SIZE],
    char oldAttrName[ATTR_SIZE],
    char newAttrName[ATTR_SIZE]
) {

    // Attributes of system catalogs cannot be renamed
    if (
        strcmp(relName, RELCAT_RELNAME) == 0 ||
        strcmp(relName, ATTRCAT_RELNAME) == 0
    ) {
        return E_NOTPERMITTED;
    }

    // Relation must be closed before altering its schema
    if (OpenRelTable::getRelId(relName) != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    // Actual attribute catalog modification happens below
    return BlockAccess::renameAttribute(
        relName,
        oldAttrName,
        newAttrName
    );
}

int Schema::deleteRel(char relName[ATTR_SIZE]) {

    // System catalogs cannot be deleted
    if (strcmp(relName, RELCAT_RELNAME) == 0 ||
        strcmp(relName, ATTRCAT_RELNAME) == 0) {
        return E_NOTPERMITTED;
    }

    // Check whether the relation is open
    int relId = OpenRelTable::getRelId(
        relName
    );

    if (relId >= 0) {
        return E_RELOPEN;
    }

    // Delete the relation
    return BlockAccess::deleteRelation(relName);
}



int Schema::createRel(
    char relName[],
    int nAttrs,
    char attrs[][ATTR_SIZE],
    int attrtype[]
) {
    // STEP 1: Check whether relation already exists

    Attribute relNameAttr;
    strcpy(relNameAttr.sVal, relName);

    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    char attrName[] = RELCAT_ATTR_RELNAME;

    RecId recId = BlockAccess::linearSearch(
        RELCAT_RELID,
        attrName,
        relNameAttr,
        EQ
    );

    if (recId.block != -1) {
        return E_RELEXIST;
    }


    // STEP 2: Check duplicate attribute names

    for (int i = 0; i < nAttrs; i++) {
        for (int j = i + 1; j < nAttrs; j++) {
            if (strcmp(attrs[i], attrs[j]) == 0) {
                return E_DUPLICATEATTR;
            }
        }
    }


    // STEP 3: Prepare Relation Catalog record

    Attribute relCatRecord[RELCAT_NO_ATTRS] = {};

    strcpy(
        relCatRecord[RELCAT_REL_NAME_INDEX].sVal,
        relName
    );

    relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal = nAttrs;
    relCatRecord[RELCAT_NO_RECORDS_INDEX].nVal = 0;
    relCatRecord[RELCAT_FIRST_BLOCK_INDEX].nVal = -1;
    relCatRecord[RELCAT_LAST_BLOCK_INDEX].nVal = -1;

    relCatRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal =
        (BLOCK_SIZE - HEADER_SIZE) / (ATTR_SIZE * nAttrs + 1);


    // STEP 4: Insert into Relation Catalog

    int retVal = BlockAccess::insert(
        RELCAT_RELID,
        relCatRecord
    );

    if (retVal != SUCCESS) {
        return retVal;
    }


    // STEP 5: Insert attributes into Attribute Catalog

    for (int i = 0; i < nAttrs; i++) {

        Attribute attrCatRecord[ATTRCAT_NO_ATTRS] = {};

        strcpy(
            attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,
            relName
        );

        strcpy(
            attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,
            attrs[i]
        );

        attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal =
            attrtype[i];

        attrCatRecord[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = -1;

        attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal = -1;

        attrCatRecord[ATTRCAT_OFFSET_INDEX].nVal = i;

        retVal = BlockAccess::insert(
            ATTRCAT_RELID,
            attrCatRecord
        );

        if (retVal != SUCCESS) {
            Schema::deleteRel(relName);
            return E_DISKFULL;
        }
    }

    return SUCCESS;
}
