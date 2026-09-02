#include "OpenRelTable.h"
#include <cstring>
#include <cstdlib>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

const int STUDENTS_RELID = 2;

OpenRelTable::OpenRelTable()
{
    /*
     * Initially no relation is loaded into the caches.
     *
     * relCache[i]  stores relation information.
     * attrCache[i] stores the attribute list of that relation.
     */
    for (int i = 0; i < MAX_OPEN; ++i) {
    RelCacheTable::relCache[i] = nullptr;
    AttrCacheTable::attrCache[i] = nullptr;

    tableMetaInfo[i].free = true;
    strcpy(tableMetaInfo[i].relName, "");
}


    /*
     * RELCAT_BLOCK is the disk block containing
     * the Relation Catalog.
     */
    RecBuffer relCatBlock(RELCAT_BLOCK);


    /*
     * Create space to hold one record from RELATIONCAT.
     *
     * A Relation Catalog record contains:
     * relation name, number of attributes, number of records,
     * first block, last block and number of slots per block.
     */
    Attribute relCatRecord[RELCAT_NO_ATTRS];


    /*
     * Read the record that describes RELATIONCAT itself.
     *
     * RELCAT_SLOTNUM_FOR_RELCAT is the slot number
     * where RELATIONCAT's own catalog entry is stored.
     */
    relCatBlock.getRecord(
        relCatRecord,
        RELCAT_SLOTNUM_FOR_RELCAT
    );


    /*
     * Create a temporary relation-cache entry.
     * This contains the actual Relation Catalog information
     * along with extra cache information such as recId.
     */
    struct RelCacheEntry relCacheEntry;


    /*
     * Convert the raw Attribute[] record obtained from disk
     * into the easier-to-use RelCatEntry structure.
     */
    RelCacheTable::recordToRelCatEntry(
        relCatRecord,
        &relCacheEntry.relCatEntry
    );


    /*
     * Remember where this Relation Catalog record
     * physically exists on disk.
     */
    relCacheEntry.recId.block = RELCAT_BLOCK;
    relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

    relCacheEntry.dirty = false;
    relCacheEntry.searchIndex = {-1, -1};


    /*
     * Allocate permanent memory for the RELATIONCAT cache entry.
     *
     * RELCAT_RELID is 0, so this effectively creates:
     *
     * relCache[0] -> RELATIONCAT
     */
    RelCacheTable::relCache[RELCAT_RELID] =
        (struct RelCacheEntry *)malloc(sizeof(RelCacheEntry));


    /*
     * Copy the prepared RELATIONCAT information
     * into the newly allocated cache entry.
     */
    *(RelCacheTable::relCache[RELCAT_RELID]) =
        relCacheEntry;


        // Read the RELATIONCAT record that describes ATTRIBUTECAT.
    Attribute attrCatRelRecord[RELCAT_NO_ATTRS];

    relCatBlock.getRecord(
        attrCatRelRecord,
        RELCAT_SLOTNUM_FOR_ATTRCAT
    );

    struct RelCacheEntry attrCatRelCacheEntry;

    // Convert the raw catalog record into a RelCatEntry.
    RelCacheTable::recordToRelCatEntry(
        attrCatRelRecord,
        &attrCatRelCacheEntry.relCatEntry
    );

    // Store where this record exists in RELATIONCAT.
    attrCatRelCacheEntry.recId.block = RELCAT_BLOCK;
    attrCatRelCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

    attrCatRelCacheEntry.dirty = false;
    attrCatRelCacheEntry.searchIndex = {-1, -1};

    // Store ATTRIBUTECAT in relation cache position 1.
    RelCacheTable::relCache[ATTRCAT_RELID] =
        (struct RelCacheEntry *)malloc(sizeof(RelCacheEntry));

    *(RelCacheTable::relCache[ATTRCAT_RELID]) =
        attrCatRelCacheEntry;


     // Attribute Catalog block contains the attribute descriptions.
    RecBuffer attrCatBlock(ATTRCAT_BLOCK);

    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

    AttrCacheEntry *head = nullptr;
    AttrCacheEntry *prev = nullptr;

    // Slots 0-5 describe the six attributes of RELATIONCAT.
    for (int i = 0; i < RELCAT_NO_ATTRS; i++)
    {
        attrCatBlock.getRecord(attrCatRecord, i);

        AttrCacheEntry *newEntry =
            (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));

        AttrCacheTable::recordToAttrCatEntry(
            attrCatRecord,
            &newEntry->attrCatEntry
        );

        newEntry->recId.block = ATTRCAT_BLOCK;
        newEntry->recId.slot = i;

        newEntry->dirty = false;
        newEntry->searchIndex = {-1, -1};

        newEntry->next = nullptr;
        if (head == nullptr)
        {
            head = newEntry;
        }
        else
        {
            prev->next = newEntry;
        }

        prev = newEntry;
    }

    // attrCache[0] points to RELATIONCAT's attribute linked list.
    AttrCacheTable::attrCache[RELCAT_RELID] = head;   


    AttrCacheEntry *attrCatHead = nullptr;
    AttrCacheEntry *attrCatPrev = nullptr;

    // Slots 6-11 describe the six attributes of ATTRIBUTECAT.
    for (int i = 0; i < ATTRCAT_NO_ATTRS; i++)
    {
        int slotNum = RELCAT_NO_ATTRS + i;

        attrCatBlock.getRecord(attrCatRecord, slotNum);

        AttrCacheEntry *newEntry =
            (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));

        AttrCacheTable::recordToAttrCatEntry(
            attrCatRecord,
            &newEntry->attrCatEntry
        );

        newEntry->recId.block = ATTRCAT_BLOCK;
        newEntry->recId.slot = slotNum;

        newEntry->dirty = false;
        newEntry->searchIndex = {-1, -1};

        newEntry->next = nullptr;

        if (attrCatHead == nullptr)
        {
            attrCatHead = newEntry;
        }
        else
        {
            attrCatPrev->next = newEntry;
        }

        attrCatPrev = newEntry;
    }

    // attrCache[1] points to ATTRIBUTECAT's attribute list.
    AttrCacheTable::attrCache[ATTRCAT_RELID] = attrCatHead;

    // stage 3 modification
    

    HeadInfo relCatHeader;
    relCatBlock.getHeader(&relCatHeader);

    // Find the Students record inside RELATIONCAT.
    for (int i = 0; i < relCatHeader.numEntries; i++)
    {
        Attribute record[RELCAT_NO_ATTRS];
        relCatBlock.getRecord(record, i);

        if (strcmp(
                record[RELCAT_REL_NAME_INDEX].sVal,
                "Students"
            ) == 0)
        {
            RelCacheEntry studentEntry;

            RelCacheTable::recordToRelCatEntry(
                record,
                &studentEntry.relCatEntry
            );

            studentEntry.recId.block = RELCAT_BLOCK;
            studentEntry.recId.slot = i;

            RelCacheTable::relCache[STUDENTS_RELID] =
                (RelCacheEntry *)malloc(sizeof(RelCacheEntry));

            *(RelCacheTable::relCache[STUDENTS_RELID]) =
                studentEntry;

            break;
        }
    }
    //modification for stage 3
    //for attrcat, find the attributes of Students in ATTRIBUTECAT
    AttrCacheEntry *studentHead = nullptr;
    AttrCacheEntry *studentPrev = nullptr;

    int attrBlockNum = ATTRCAT_BLOCK;

    // Search Attribute Catalog for attributes belonging to Students.
    while (attrBlockNum != -1)
    {
        RecBuffer block(attrBlockNum);

        HeadInfo header;
        block.getHeader(&header);

        for (int i = 0; i < header.numEntries; i++)
        {
            Attribute record[ATTRCAT_NO_ATTRS];
            block.getRecord(record, i);

            if (strcmp(
                    record[ATTRCAT_REL_NAME_INDEX].sVal,
                    "Students"
                ) == 0)
            {
                AttrCacheEntry *newEntry =
                    (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));

                AttrCacheTable::recordToAttrCatEntry(
                    record,
                    &newEntry->attrCatEntry
                );

                newEntry->recId.block = attrBlockNum;
                newEntry->recId.slot = i;

                newEntry->dirty = false;
                newEntry->searchIndex = {-1, -1};

                newEntry->next = nullptr;
                
                if (studentHead == nullptr)
                    studentHead = newEntry;
                else
                    studentPrev->next = newEntry;

                studentPrev = newEntry;
            }
        }

        // Move to the next Attribute Catalog block.
        attrBlockNum = header.rblock;
}

AttrCacheTable::attrCache[STUDENTS_RELID] = studentHead;



tableMetaInfo[RELCAT_RELID].free = false;
strcpy(tableMetaInfo[RELCAT_RELID].relName, RELCAT_RELNAME);

tableMetaInfo[ATTRCAT_RELID].free = false;
strcpy(tableMetaInfo[ATTRCAT_RELID].relName, ATTRCAT_RELNAME);

}

int OpenRelTable::getFreeOpenRelTableEntry() {
  for (int i = 0; i < MAX_OPEN; i++) {
    if (tableMetaInfo[i].free) {
      return i;
    }
  }

  return E_CACHEFULL;
}

int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {
  for (int i = 0; i < MAX_OPEN; i++) {

    if (!tableMetaInfo[i].free &&
        strcmp(tableMetaInfo[i].relName, relName) == 0) {
      return i;
    }
  }

  return E_RELNOTOPEN;
}

int OpenRelTable::openRel(char relName[ATTR_SIZE]) {

  // ----------------------------------------------------
  // 1. Check whether the relation is already open
  // ----------------------------------------------------
  int relId = OpenRelTable::getRelId(relName);

  if (relId >= 0) {
    return relId;
  }


  // ----------------------------------------------------
  // 2. Find a free slot in OpenRelTable
  // ----------------------------------------------------
  relId = OpenRelTable::getFreeOpenRelTableEntry();

  if (relId == E_CACHEFULL) {
    return E_CACHEFULL;
  }


  // Attribute value used for searching RELCAT and ATTRCAT
  Attribute relNameAttr;
  strcpy(relNameAttr.sVal, relName);


  // ====================================================
  // 3. SET UP RELATION CACHE ENTRY
  // ====================================================

  // Start RELCAT search from the beginning
  RelCacheTable::resetSearchIndex(RELCAT_RELID);

  RecId relcatRecId =
      BlockAccess::linearSearch(
          RELCAT_RELID,
          RELCAT_ATTR_RELNAME,
          relNameAttr,
          EQ
      );

  // Relation does not exist in RELCAT
  if (relcatRecId.block == -1 && relcatRecId.slot == -1) {
    return E_RELNOTEXIST;
  }


  // Read the RELCAT record
  RecBuffer relCatBlock(relcatRecId.block);

  Attribute relCatRecord[RELCAT_NO_ATTRS];

  relCatBlock.getRecord(
      relCatRecord,
      relcatRecId.slot
  );


  // Allocate Relation Cache entry
  RelCacheEntry *relCacheEntry =
      (RelCacheEntry *)malloc(sizeof(RelCacheEntry));


  // Convert raw RELCAT record -> RelCatEntry
  RelCacheTable::recordToRelCatEntry(
      relCatRecord,
      &relCacheEntry->relCatEntry
  );


  // Initialize cache-specific information
  relCacheEntry->recId = relcatRecId;

  relCacheEntry->dirty = false;

  relCacheEntry->searchIndex.block = -1;
  relCacheEntry->searchIndex.slot = -1;


  // Store it in RelCacheTable
  RelCacheTable::relCache[relId] = relCacheEntry;


  // ====================================================
  // 4. SET UP ATTRIBUTE CACHE LINKED LIST
  // ====================================================

  AttrCacheEntry *listHead = nullptr;
  AttrCacheEntry *last = nullptr;


  // Start ATTRCAT search from the beginning
  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);


  while (true) {

    RecId attrcatRecId =
        BlockAccess::linearSearch(
            ATTRCAT_RELID,
            ATTRCAT_ATTR_RELNAME,
            relNameAttr,
            EQ
        );


    // No more attributes belonging to this relation
    if (attrcatRecId.block == -1 &&
        attrcatRecId.slot == -1) {
      break;
    }


    // Read this ATTRCAT record
    RecBuffer attrCatBlock(attrcatRecId.block);

    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

    attrCatBlock.getRecord(
        attrCatRecord,
        attrcatRecId.slot
    );


    // Allocate a new linked-list node
    AttrCacheEntry *newEntry =
        (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));


    // Convert raw ATTRCAT record -> AttrCatEntry
    AttrCacheTable::recordToAttrCatEntry(
        attrCatRecord,
        &newEntry->attrCatEntry
    );


    // Initialize cache-specific information
    newEntry->recId = attrcatRecId;

    newEntry->dirty = false;

    newEntry->searchIndex.block = -1;
    newEntry->searchIndex.index = -1;

    newEntry->next = nullptr;


    // Add to linked list
    if (listHead == nullptr) {

      listHead = newEntry;
      last = newEntry;

    } else {

      last->next = newEntry;
      last = newEntry;
    }
  }


  // Store head of linked list in AttrCacheTable
  AttrCacheTable::attrCache[relId] = listHead;


  // ====================================================
  // 5. UPDATE OPEN RELATION TABLE
  // ====================================================

  tableMetaInfo[relId].free = false;

  strcpy(
      tableMetaInfo[relId].relName,
      relName
  );


  // Return the newly assigned relation id
  return relId;
}

int OpenRelTable::closeRel(int relId) {

  // RELATIONCAT and ATTRIBUTECAT cannot be closed normally
  if (relId == RELCAT_RELID || relId == ATTRCAT_RELID) {
    return E_NOTPERMITTED;
  }

  // Check whether relId is valid
  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  // Nothing is open in this slot
  if (tableMetaInfo[relId].free) {
    return E_RELNOTOPEN;
  }


  // --------------------------------------------------
  // 1. Free Relation Cache entry
  // --------------------------------------------------
  free(RelCacheTable::relCache[relId]);

  RelCacheTable::relCache[relId] = nullptr;


  // --------------------------------------------------
  // 2. Free Attribute Cache linked list
  // --------------------------------------------------
  AttrCacheEntry *current =
      AttrCacheTable::attrCache[relId];

  while (current != nullptr) {

    // Save next before freeing current
    AttrCacheEntry *next = current->next;

    free(current);

    current = next;
  }

  AttrCacheTable::attrCache[relId] = nullptr;


  // --------------------------------------------------
  // 3. Mark OpenRelTable slot as free
  // --------------------------------------------------
  tableMetaInfo[relId].free = true;

  strcpy(tableMetaInfo[relId].relName, "");


  return SUCCESS;
}

OpenRelTable::~OpenRelTable()
{
    for (int relId = 0; relId < MAX_OPEN; relId++)
    {
        // Free the linked list of attribute-cache entries.
        AttrCacheEntry *entry = AttrCacheTable::attrCache[relId];

        while (entry != nullptr)
        {
            AttrCacheEntry *nextEntry = entry->next;
            free(entry);
            entry = nextEntry;
        }

        AttrCacheTable::attrCache[relId] = nullptr;

        // Free the relation-cache entry.
        if (RelCacheTable::relCache[relId] != nullptr)
        {
            free(RelCacheTable::relCache[relId]);
            RelCacheTable::relCache[relId] = nullptr;
        }
    }
}
