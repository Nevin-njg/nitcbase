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
    // 1. Find relId of source relation
    int srcRelId = OpenRelTable::getRelId(srcRel);

    if (srcRelId == E_RELNOTOPEN)
    {
        return E_RELNOTOPEN;
    }


    // 2. Get metadata of the attribute used in WHERE
    AttrCatEntry attrCatEntry;

    int ret = AttrCacheTable::getAttrCatEntry(
        srcRelId,
        attr,
        &attrCatEntry
    );

    if (ret != SUCCESS)
    {
        return ret;
    }


    // 3. Convert strVal into proper Attribute type
    Attribute attrVal;

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


    // 4. Start search from beginning
    RelCacheTable::resetSearchIndex(srcRelId);


    // 5. Get relation metadata
    RelCatEntry relCatEntry;

    RelCacheTable::getRelCatEntry(
        srcRelId,
        &relCatEntry
    );


    // 6. Print column names
    printf("|");

    for (int i = 0; i < relCatEntry.numAttrs; i++)
    {
        AttrCatEntry columnEntry;

        AttrCacheTable::getAttrCatEntry(
            srcRelId,
            i,
            &columnEntry
        );

        printf(" %s |", columnEntry.attrName);
    }

    printf("\n");


    // 7. Repeatedly find matching records
    while (true)
    {
        RecId searchRes = BlockAccess::linearSearch(
            srcRelId,
            attr,
            attrVal,
            op
        );

        // No more matches
        if (searchRes.block == -1 &&
            searchRes.slot == -1)
        {
            break;
        }


        // 8. Read actual matching record from buffer
        RecBuffer block(searchRes.block);

        Attribute record[relCatEntry.numAttrs];

        block.getRecord(
            record,
            searchRes.slot
        );


        // 9. Print the record
        printf("|");

        for (int i = 0; i < relCatEntry.numAttrs; i++)
        {
            AttrCatEntry columnEntry;

            AttrCacheTable::getAttrCatEntry(
                srcRelId,
                i,
                &columnEntry
            );

            if (columnEntry.attrType == NUMBER)
            {
                printf(" %g |", record[i].nVal);
            }
            else
            {
                printf(" %s |", record[i].sVal);
            }
        }

        printf("\n");
    }


    return SUCCESS;
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