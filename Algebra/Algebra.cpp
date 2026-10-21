#include "Algebra.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>


// Returns true if the given string represents a number
static bool isNumber(char *str)
{
    int len;
    float ignore;

    int ret = sscanf(str, "%f %n", &ignore, &len);

    return ret == 1 && len == strlen(str);
}

int Algebra::select(
    char srcRel[ATTR_SIZE],
    char targetRel[ATTR_SIZE],
    char attr[ATTR_SIZE],
    int op,
    char strVal[ATTR_SIZE])
{
    // 1. Find source relation
    int srcRelId = OpenRelTable::getRelId(srcRel);

    if (srcRelId == E_RELNOTOPEN)
        return E_RELNOTOPEN;


    // 2. Get attribute metadata for WHERE condition
    AttrCatEntry attrCatEntry;

    int ret = AttrCacheTable::getAttrCatEntry(
        srcRelId,
        attr,
        &attrCatEntry
    );

    if (ret != SUCCESS)
        return ret;


    // 3. Convert condition value to correct type
    Attribute attrVal{};

    if (attrCatEntry.attrType == NUMBER)
    {
        if (isNumber(strVal))
        {
            attrVal.nVal = atof(strVal);
        }
        else
        {
            return E_ATTRTYPEMISMATCH;
        }
    }
    else if (attrCatEntry.attrType == STRING)
    {
        strcpy(attrVal.sVal, strVal);
    }


    // 4. Get source relation metadata
    RelCatEntry relCatEntry;

    ret = RelCacheTable::getRelCatEntry(
        srcRelId,
        &relCatEntry
    );

    if (ret != SUCCESS)
        return ret;

    int nAttrs = relCatEntry.numAttrs;


    // 5. Get all source attribute names and types
    char attrNames[nAttrs][ATTR_SIZE];
    int attrTypes[nAttrs];

    for (int i = 0; i < nAttrs; i++)
    {
        AttrCatEntry columnEntry;

        ret = AttrCacheTable::getAttrCatEntry(
            srcRelId,
            i,
            &columnEntry
        );

        if (ret != SUCCESS)
            return ret;

        strcpy(attrNames[i], columnEntry.attrName);

        attrTypes[i] = columnEntry.attrType;
    }


    // 6. Create target relation
    ret = Schema::createRel(
        targetRel,
        nAttrs,
        attrNames,
        attrTypes
    );

    if (ret != SUCCESS)
        return ret;


    // 7. Open target relation
    int targetRelId = OpenRelTable::openRel(targetRel);

    if (targetRelId < 0)
    {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }


    // 8. Reset source search index
    ret = RelCacheTable::resetSearchIndex(srcRelId);

    if (ret != SUCCESS)
    {
        if (Schema::closeRel(targetRel) == SUCCESS)
            Schema::deleteRel(targetRel);

        return ret;
    }


    // 9. Retrieve and insert matching records
    Attribute record[nAttrs];

    while (true)
    {
        ret = BlockAccess::search(
            srcRelId,
            record,
            attr,
            attrVal,
            op
        );

        // No more matching records
        if (ret == E_NOTFOUND)
            break;

        // Handle search error
        if (ret != SUCCESS)
        {
            if (Schema::closeRel(targetRel) == SUCCESS)
                Schema::deleteRel(targetRel);

            return ret;
        }

        // Insert matching record into target
        ret = BlockAccess::insert(
            targetRelId,
            record
        );

        if (ret != SUCCESS)
        {
            if (Schema::closeRel(targetRel) == SUCCESS)
                Schema::deleteRel(targetRel);

            return ret;
        }
    }


    // 10. Close target relation
    return Schema::closeRel(targetRel);
}


int Algebra::insert(
    char relName[ATTR_SIZE],
    int nAttrs,
    char record[][ATTR_SIZE]) {

  // --------------------------------------------------
  // 1. Cannot insert into catalog relations
  // --------------------------------------------------
  if (strcmp(relName, RELCAT_RELNAME) == 0 ||
      strcmp(relName, ATTRCAT_RELNAME) == 0) {

    return E_NOTPERMITTED;
  }


  // --------------------------------------------------
  // 2. Get relation ID
  // --------------------------------------------------
  int relId = OpenRelTable::getRelId(relName);

  if (relId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }


  // --------------------------------------------------
  // 3. Get relation catalog entry
  // --------------------------------------------------
  RelCatEntry relCatEntry;

  int status =
      RelCacheTable::getRelCatEntry(relId, &relCatEntry);

  if (status != SUCCESS) {
    return status;
  }


  // --------------------------------------------------
  // 4. Check number of attributes
  // --------------------------------------------------
  if (relCatEntry.numAttrs != nAttrs) {
    return E_NATTRMISMATCH;
  }


  // --------------------------------------------------
  // 5. Convert input strings to Attribute[]
  // --------------------------------------------------
  Attribute recordValues[nAttrs];

  for (int i = 0; i < nAttrs; i++) {

    AttrCatEntry attrCatEntry;

    status =
        AttrCacheTable::getAttrCatEntry(
            relId,
            i,
            &attrCatEntry
        );

    if (status != SUCCESS) {
      return status;
    }


    // ----------------------------------------------
    // NUMBER attribute
    // ----------------------------------------------
    if (attrCatEntry.attrType == NUMBER) {

      if (!isNumber(record[i])) {
        return E_ATTRTYPEMISMATCH;
      }

      recordValues[i].nVal =
          atof(record[i]);
    }


    // ----------------------------------------------
    // STRING attribute
    // ----------------------------------------------
    else if (attrCatEntry.attrType == STRING) {

      strcpy(
          recordValues[i].sVal,
          record[i]
      );
    }
  }


  // --------------------------------------------------
  // 6. Pass converted record to Block Access Layer
  // --------------------------------------------------
  return BlockAccess::insert(
      relId,
      recordValues
  );
}

int Algebra::project(
    char srcRel[ATTR_SIZE],
    char targetRel[ATTR_SIZE])
{
    // 1. Get source relation ID
    int srcRelId = OpenRelTable::getRelId(srcRel);

    if (srcRelId == E_RELNOTOPEN)
        return E_RELNOTOPEN;


    // 2. Get source relation metadata
    RelCatEntry relCatEntry;

    int ret = RelCacheTable::getRelCatEntry(
        srcRelId, &relCatEntry
    );

    if (ret != SUCCESS)
        return ret;

    int nAttrs = relCatEntry.numAttrs;


    // 3. Get attribute names and types
    char attrNames[nAttrs][ATTR_SIZE];
    int attrTypes[nAttrs];

    for (int i = 0; i < nAttrs; i++)
    {
        AttrCatEntry attrCatEntry;

        ret = AttrCacheTable::getAttrCatEntry(
            srcRelId, i, &attrCatEntry
        );

        if (ret != SUCCESS)
            return ret;

        strcpy(attrNames[i], attrCatEntry.attrName);
        attrTypes[i] = attrCatEntry.attrType;
    }


    // 4. Create target relation
    ret = Schema::createRel(
        targetRel, nAttrs, attrNames, attrTypes
    );

    if (ret != SUCCESS)
        return ret;


    // 5. Open target relation
    int targetRelId = OpenRelTable::openRel(targetRel);

    if (targetRelId < 0)
    {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }


    // 6. Reset source search index
    ret = RelCacheTable::resetSearchIndex(srcRelId);

    if (ret != SUCCESS)
    {
        if (Schema::closeRel(targetRel) == SUCCESS)
            Schema::deleteRel(targetRel);

        return ret;
    }


    // 7. Copy all records
    Attribute record[nAttrs];

    while (true)
    {
        ret = BlockAccess::project(srcRelId, record);

        if (ret == E_NOTFOUND)
            break;

        if (ret != SUCCESS)
        {
            if (Schema::closeRel(targetRel) == SUCCESS)
                Schema::deleteRel(targetRel);

            return ret;
        }

        ret = BlockAccess::insert(
            targetRelId, record
        );

        if (ret != SUCCESS)
        {
            if (Schema::closeRel(targetRel) == SUCCESS)
                Schema::deleteRel(targetRel);

            return ret;
        }
    }


    // 8. Close target relation
    return Schema::closeRel(targetRel);
}



int Algebra::project(
    char srcRel[ATTR_SIZE],
    char targetRel[ATTR_SIZE],
    int tar_nAttrs,
    char tar_Attrs[][ATTR_SIZE])
{
    // 1. Find source relation
    int srcRelId = OpenRelTable::getRelId(srcRel);

    if (srcRelId == E_RELNOTOPEN)
        return E_RELNOTOPEN;

    // 2. Get source relation metadata
    RelCatEntry relCatEntry;

    int ret = RelCacheTable::getRelCatEntry(
        srcRelId, &relCatEntry
    );

    if (ret != SUCCESS)
        return ret;

    int src_nAttrs = relCatEntry.numAttrs;

    if (tar_nAttrs <= 0)
        return E_NATTRMISMATCH;

    // 3. Store selected attribute offsets and types
    int attrOffsets[tar_nAttrs];
    int attrTypes[tar_nAttrs];

    for (int i = 0; i < tar_nAttrs; i++)
    {
        AttrCatEntry attrCatEntry;

        ret = AttrCacheTable::getAttrCatEntry(
            srcRelId,
            tar_Attrs[i],
            &attrCatEntry
        );

        if (ret != SUCCESS)
            return ret;

        attrOffsets[i] = attrCatEntry.offset;
        attrTypes[i] = attrCatEntry.attrType;
    }

    // 4. Create target relation
    ret = Schema::createRel(
        targetRel,
        tar_nAttrs,
        tar_Attrs,
        attrTypes
    );

    if (ret != SUCCESS)
        return ret;

    // 5. Open target relation
    int targetRelId = OpenRelTable::openRel(targetRel);

    if (targetRelId < 0)
    {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    // 6. Reset source search index
    ret = RelCacheTable::resetSearchIndex(srcRelId);

    if (ret != SUCCESS)
    {
        if (Schema::closeRel(targetRel) == SUCCESS)
            Schema::deleteRel(targetRel);

        return ret;
    }

    Attribute record[src_nAttrs];
    Attribute projectedRecord[tar_nAttrs];

    // 7. Retrieve records one by one
    while (true)
    {
        ret = BlockAccess::project(srcRelId, record);

        if (ret == E_NOTFOUND)
            break;

        if (ret != SUCCESS)
        {
            if (Schema::closeRel(targetRel) == SUCCESS)
                Schema::deleteRel(targetRel);

            return ret;
        }

        // 8. Copy only selected attributes
        for (int i = 0; i < tar_nAttrs; i++)
        {
            projectedRecord[i] = record[attrOffsets[i]];
        }

        // 9. Insert projected record
        ret = BlockAccess::insert(
            targetRelId,
            projectedRecord
        );

        if (ret != SUCCESS)
        {
            if (Schema::closeRel(targetRel) == SUCCESS)
                Schema::deleteRel(targetRel);

            return ret;
        }
    }

    // 10. Close target relation
    return Schema::closeRel(targetRel);
}