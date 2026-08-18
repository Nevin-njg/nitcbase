#include "Buffer/BlockBuffer.h"
#include "Disk_Class/Disk.h"
#include "Cache/OpenRelTable.h"

#include <cstdio>


int main()
{
    // Disk must be initialized first.
    Disk disk_run;

    // Initializes the disk buffer.
    StaticBuffer buffer;

    // Loads RELATIONCAT and ATTRIBUTECAT into the caches.
    OpenRelTable cache;

    const int STUDENTS_RELID = 2;
    // Print RELATIONCAT, ATTRIBUTECAT and Students from the cache.
    for (int relId = RELCAT_RELID;
        relId <= STUDENTS_RELID;
        relId++)
    {
        RelCatEntry relEntry;

        int status =
            RelCacheTable::getRelCatEntry(
                relId,
                &relEntry
            );

        if (status != SUCCESS)
        {
            printf("Failed to get relation cache entry.\n");
            return 1;
        }


        printf("Relation: %s\n", relEntry.relName);


        // Get every attribute of this relation from attrCache.
        for (int attrOffset = 0;
             attrOffset < relEntry.numAttrs;
             attrOffset++)
        {
            AttrCatEntry attrEntry;

            status =
                AttrCacheTable::getAttrCatEntry(
                    relId,
                    attrOffset,
                    &attrEntry
                );

            if (status != SUCCESS)
            {
                printf("Failed to get attribute cache entry.\n");
                return 1;
            }


            const char *attributeType =
                (attrEntry.attrType == NUMBER)
                    ? "NUM"
                    : "STR";


            printf(
                "  %s: %s\n",
                attrEntry.attrName,
                attributeType
            );
        }


        printf("\n");
    }


    return 0;
}